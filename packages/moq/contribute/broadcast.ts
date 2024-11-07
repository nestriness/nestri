import * as Transfork from "../transfork"
import * as Catalog from "../karp/catalog"
import * as Audio from "./audio"
import * as Video from "./video"

import { isAudioTrackSettings, isVideoTrackSettings } from "../common/settings"

export interface BroadcastConfig {
	path: string[]
	media: MediaStream
	id?: number

	audio?: AudioEncoderConfig
	video?: VideoEncoderConfig
}

export interface BroadcastConfigTrack {
	codec: string
	bitrate: number
}

export class Broadcast {
	#config: BroadcastConfig
	#path: string[]

	constructor(config: BroadcastConfig) {
		const id = config.id || new Date().getTime() / 1000

		this.#config = config
		this.#path = config.path.concat(id.toString())
	}

	async publish(connection: Transfork.Connection) {
		const broadcast: Catalog.Broadcast = { path: this.#config.path, audio: [], video: [] }

		for (const media of this.#config.media.getTracks()) {
			const settings = media.getSettings()

			const info = {
				name: media.id, // TODO way too verbose
				priority: media.kind == "video" ? 1 : 2,
			}

			const track = new Transfork.Track(this.#config.path.concat(info.name), info.priority)

			if (isVideoTrackSettings(settings)) {
				if (!this.#config.video) {
					throw new Error("no video configuration provided")
				}

				const encoder = new Video.Encoder(this.#config.video)
				const packer = new Video.Packer(media as MediaStreamVideoTrack, encoder, track)

				// TODO handle error
				packer.run().catch((err) => console.error("failed to run video packer: ", err))

				const decoder = await encoder.decoderConfig()
				const description = decoder.description ? new Uint8Array(decoder.description as ArrayBuffer) : undefined

				const video: Catalog.Video = {
					track: info,
					codec: decoder.codec,
					description: description,
					resolution: { width: settings.width, height: settings.height },
					frame_rate: settings.frameRate,
					bitrate: this.#config.video.bitrate,
				}

				broadcast.video.push(video)
			} else if (isAudioTrackSettings(settings)) {
				if (!this.#config.audio) {
					throw new Error("no audio configuration provided")
				}

				const encoder = new Audio.Encoder(this.#config.audio)
				const packer = new Audio.Packer(media as MediaStreamAudioTrack, encoder, track)
				packer.run().catch((err) => console.error("failed to run audio packer: ", err)) // TODO handle error

				const decoder = await encoder.decoderConfig()

				const audio: Catalog.Audio = {
					track: info,
					codec: decoder.codec,
					sample_rate: settings.sampleRate,
					channel_count: settings.channelCount,
					bitrate: this.#config.audio.bitrate,
				}

				broadcast.audio.push(audio)
			} else {
				throw new Error(`unknown track type: ${media.kind}`)
			}

			connection.publish(track.reader())
		}

		const track = new Transfork.Track(this.#config.path.concat("catalog.json"), 0)
		track.appendGroup().writeFrames(Catalog.encode(broadcast))

		connection.publish(track.reader())
	}

	close() {}
}
