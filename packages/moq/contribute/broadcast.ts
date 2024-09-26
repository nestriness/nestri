import { Connection, SubscribeRecv } from "../transport"
import { asError } from "../common/error"
import { Segment } from "./segment"
import { Track } from "./track"
import * as Catalog from "../media/catalog"

import { isAudioTrackSettings, isVideoTrackSettings } from "../common/settings"

export interface BroadcastConfig {
	namespace: string
	connection: Connection
	media: MediaStream

	audio?: AudioEncoderConfig
	video?: VideoEncoderConfig
}

export interface BroadcastConfigTrack {
	codec: string
	bitrate: number
}

export class Broadcast {
	#tracks = new Map<string, Track>()

	readonly config: BroadcastConfig
	readonly catalog: Catalog.Root
	readonly connection: Connection
	readonly namespace: string

	#running: Promise<void>

	constructor(config: BroadcastConfig) {
		this.connection = config.connection
		this.config = config
		this.namespace = config.namespace

		const tracks: Catalog.Track[] = []

		for (const media of this.config.media.getTracks()) {
			const track = new Track(media, config)
			this.#tracks.set(track.name, track)

			const settings = media.getSettings()

			if (isVideoTrackSettings(settings)) {
				if (!config.video) {
					throw new Error("no video configuration provided")
				}

				const video: Catalog.VideoTrack = {
					namespace: this.namespace,
					name: `${track.name}.m4s`,
					initTrack: `${track.name}.mp4`,
					selectionParams: {
						mimeType: "video/mp4",
						codec: config.video.codec,
						width: settings.width,
						height: settings.height,
						framerate: settings.frameRate,
						bitrate: config.video.bitrate,
					},
				}

				tracks.push(video)
			} else if (isAudioTrackSettings(settings)) {
				if (!config.audio) {
					throw new Error("no audio configuration provided")
				}

				const audio: Catalog.AudioTrack = {
					namespace: this.namespace,
					name: `${track.name}.m4s`,
					initTrack: `${track.name}.mp4`,
					selectionParams: {
						mimeType: "audio/ogg",
						codec: config.audio.codec,
						samplerate: settings.sampleRate,
						//sampleSize: settings.sampleSize,
						channelConfig: `${settings.channelCount}`,
						bitrate: config.audio.bitrate,
					},
				}

				tracks.push(audio)
			} else {
				throw new Error(`unknown track type: ${media.kind}`)
			}
		}

		this.catalog = {
			version: 1,
			streamingFormat: 1,
			streamingFormatVersion: "0.2",
			supportsDeltaUpdates: false,
			commonTrackFields: {
				packaging: "cmaf",
				renderGroup: 1,
			},
			tracks,
		}

		this.#running = this.#run()
	}

	async #run() {
		await this.connection.announce(this.namespace)

		for (;;) {
			const subscriber = await this.connection.subscribed()
			if (!subscriber) break

			// Run an async task to serve each subscription.
			this.#serveSubscribe(subscriber).catch((e) => {
				const err = asError(e)
				console.warn("failed to serve subscribe", err)
			})
		}
	}

	async #serveSubscribe(subscriber: SubscribeRecv) {
		try {
			const [base, ext] = splitExt(subscriber.track)
			if (ext === "catalog") {
				await this.#serveCatalog(subscriber, base)
			} else if (ext === "mp4") {
				await this.#serveInit(subscriber, base)
			} else if (ext === "m4s") {
				await this.#serveTrack(subscriber, base)
			} else {
				throw new Error(`unknown subscription: ${subscriber.track}`)
			}
		} catch (e) {
			const err = asError(e)
			await subscriber.close(1n, `failed to process subscribe: ${err.message}`)
		} finally {
			// TODO we can't close subscribers because there's no support for clean termination
			// await subscriber.close()
		}
	}

	async #serveCatalog(subscriber: SubscribeRecv, name: string) {
		// We only support ".catalog"
		if (name !== "") throw new Error(`unknown catalog: ${name}`)

		const bytes = Catalog.encode(this.catalog)

		// Send a SUBSCRIBE_OK
		await subscriber.ack()

		const stream = await subscriber.group({ group: 0 })
		await stream.write({ object: 0, payload: bytes })
		await stream.close()
	}

	async #serveInit(subscriber: SubscribeRecv, name: string) {
		const track = this.#tracks.get(name)
		if (!track) throw new Error(`no track with name ${subscriber.track}`)

		// Send a SUBSCRIBE_OK
		await subscriber.ack()

		const init = await track.init()

		const stream = await subscriber.group({ group: 0 })
		await stream.write({ object: 0, payload: init })
		await stream.close()
	}

	async #serveTrack(subscriber: SubscribeRecv, name: string) {
		const track = this.#tracks.get(name)
		if (!track) throw new Error(`no track with name ${subscriber.track}`)

		// Send a SUBSCRIBE_OK
		await subscriber.ack()

		const segments = track.segments().getReader()

		for (;;) {
			const { value: segment, done } = await segments.read()
			if (done) break

			// Serve the segment and log any errors that occur.
			this.#serveSegment(subscriber, segment).catch((e) => {
				const err = asError(e)
				console.warn("failed to serve segment", err)
			})
		}
	}

	async #serveSegment(subscriber: SubscribeRecv, segment: Segment) {
		// Create a new stream for each segment.
		const stream = await subscriber.group({
			group: segment.id,
			priority: 0, // TODO
		})

		let object = 0

		// Pipe the segment to the stream.
		const chunks = segment.chunks().getReader()
		for (;;) {
			const { value, done } = await chunks.read()
			if (done) break

			await stream.write({
				object,
				payload: value,
			})

			object += 1
		}

		await stream.close()
	}

	// Attach the captured video stream to the given video element.
	attach(video: HTMLVideoElement) {
		video.srcObject = this.config.media
	}

	close() {
		// TODO implement publish close
	}

	// Returns the error message when the connection is closed
	async closed(): Promise<Error> {
		try {
			await this.#running
			return new Error("closed") // clean termination
		} catch (e) {
			return asError(e)
		}
	}
}

function splitExt(s: string): [string, string] {
	const i = s.lastIndexOf(".")
	if (i < 0) throw new Error(`no extension found`)
	return [s.substring(0, i), s.substring(i + 1)]
}
