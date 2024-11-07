import { Connection } from "../transfork/connection"
import * as Catalog from "../karp/catalog"

import { Track } from "../transfork"

import * as Audio from "./audio"
import * as Video from "./video"
import { Timeline } from "./timeline"
import { GroupReader } from "../transfork/model"
import { Frame } from "../karp/frame"

// This class must be created on the main thread due to AudioContext.
export class Broadcast {
	#connection: Connection
	#catalog: Catalog.Broadcast

	// Running is a promise that resolves when the player is closed.
	// #close is called with no error, while #abort is called with an error.
	#running: Promise<void>

	// Timeline receives samples, buffering them and choosing the timestamp to render.
	#timeline = new Timeline()

	#audio?: Audio.Renderer
	#video?: Video.Renderer

	constructor(connection: Connection, catalog: Catalog.Broadcast, canvas: HTMLCanvasElement) {
		this.#connection = connection
		this.#catalog = catalog

		const running = []

		// Only configure audio is we have an audio track
		const audio = (catalog.audio || []).at(0)
		if (audio) {
			this.#audio = new Audio.Renderer(audio, this.#timeline.audio)
			running.push(this.#runAudio(audio))
		}

		const video = (catalog.video || []).at(0)
		if (video) {
			this.#video = new Video.Renderer(video, canvas, this.#timeline.video)
			running.push(this.#runVideo(video))
		}

		// Async work
		this.#running = Promise.race([...running])
	}

	async #runAudio(audio: Catalog.Audio) {
		const track = new Track(this.#catalog.path.concat(audio.track.name), audio.track.priority)
		const sub = await this.#connection.subscribe(track)

		try {
			for (;;) {
				const group = await Promise.race([sub.nextGroup(), this.#running])
				if (!group) break

				this.#runAudioGroup(audio, group)
					.catch(() => {})
					.finally(() => group.close())
			}
		} finally {
			sub.close()
		}
	}

	async #runVideo(video: Catalog.Video) {
		const track = new Track(this.#catalog.path.concat(video.track.name), video.track.priority)
		const sub = await this.#connection.subscribe(track)

		try {
			for (;;) {
				const group = await Promise.race([sub.nextGroup(), this.#running])
				if (!group) break

				this.#runVideoGroup(video, group)
					.catch(() => {})
					.finally(() => group.close())
			}
		} finally {
			sub.close()
		}
	}

	async #runAudioGroup(audio: Catalog.Audio, group: GroupReader) {
		const timeline = this.#timeline.audio

		// Create a queue that will contain each frame
		const queue = new TransformStream<Frame>({})
		const segment = queue.writable.getWriter()

		// Add the segment to the timeline
		const segments = timeline.segments.getWriter()
		await segments.write({
			sequence: group.id,
			frames: queue.readable,
		})
		segments.releaseLock()

		// Read each chunk, decoding the MP4 frames and adding them to the queue.
		for (;;) {
			const frame = await Frame.decode(group)
			if (!frame) break

			await segment.write(frame)
		}

		// We done.
		await segment.close()
	}

	async #runVideoGroup(video: Catalog.Video, group: GroupReader) {
		const timeline = this.#timeline.video

		// Create a queue that will contain each MP4 frame.
		const queue = new TransformStream<Frame>({})
		const segment = queue.writable.getWriter()

		// Add the segment to the timeline
		const segments = timeline.segments.getWriter()
		await segments.write({
			sequence: group.id,
			frames: queue.readable,
		})
		segments.releaseLock()

		for (;;) {
			const frame = await Frame.decode(group)
			if (!frame) break

			await segment.write(frame)
		}

		// We done.
		await segment.close()
	}

	close() {
		this.#audio?.close()
		this.#video?.close()
	}
}
