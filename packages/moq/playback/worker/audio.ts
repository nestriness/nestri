import * as Message from "./message"
import { Ring } from "../../common/ring"
import { Component, Frame } from "./timeline"
import * as MP4 from "../../media/mp4"

// This is run in a worker.
export class Renderer {
	#ring: Ring
	#timeline: Component

	#decoder!: AudioDecoder
	#stream: TransformStream<Frame, AudioData>

	constructor(config: Message.ConfigAudio, timeline: Component) {
		this.#timeline = timeline
		this.#ring = new Ring(config.ring)

		this.#stream = new TransformStream({
			start: this.#start.bind(this),
			transform: this.#transform.bind(this),
		})

		this.#run().catch(console.error)
	}

	#start(controller: TransformStreamDefaultController) {
		this.#decoder = new AudioDecoder({
			output: (frame: AudioData) => {
				controller.enqueue(frame)
			},
			error: console.warn,
		})
	}

	#transform(frame: Frame) {
		if (this.#decoder.state !== "configured") {
			const track = frame.track
			if (!MP4.isAudioTrack(track)) throw new Error("expected audio track")

			// We only support OPUS right now which doesn't need a description.
			this.#decoder.configure({
				codec: track.codec,
				sampleRate: track.audio.sample_rate,
				numberOfChannels: track.audio.channel_count,
			})
		}

		const chunk = new EncodedAudioChunk({
			type: frame.sample.is_sync ? "key" : "delta",
			timestamp: frame.sample.dts / frame.track.timescale,
			duration: frame.sample.duration,
			data: frame.sample.data,
		})

		this.#decoder.decode(chunk)
	}

	async #run() {
		const reader = this.#timeline.frames.pipeThrough(this.#stream).getReader()

		for (;;) {
			const { value: frame, done } = await reader.read()
			if (done) break

			// Write audio samples to the ring buffer, dropping when there's no space.
			const written = this.#ring.write(frame)

			if (written < frame.numberOfFrames) {
				console.warn(`droppped ${frame.numberOfFrames - written} audio samples`)
			}
		}
	}
}
