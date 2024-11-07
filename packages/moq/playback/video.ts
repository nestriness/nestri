import { Component } from "./timeline"
import * as Catalog from "../karp/catalog"
import { Frame } from "../karp/frame"

export class Renderer {
	#track: Catalog.Video
	#canvas: HTMLCanvasElement
	#timeline: Component

	#decoder!: VideoDecoder
	#queue: TransformStream<Frame, VideoFrame>

	constructor(track: Catalog.Video, canvas: HTMLCanvasElement, timeline: Component) {
		this.#track = track
		this.#canvas = canvas
		this.#timeline = timeline

		this.#queue = new TransformStream({
			start: this.#start.bind(this),
			transform: this.#transform.bind(this),
		})

		this.#run().catch((err) => console.error("failed to run video renderer: ", err))
	}

	close() {
		// TODO
	}

	async #run() {
		const reader = this.#timeline.frames.pipeThrough(this.#queue).getReader()
		for (;;) {
			const { value: frame, done } = await reader.read()
			if (done) break

			self.requestAnimationFrame(() => {
				this.#canvas.width = frame.displayWidth
				this.#canvas.height = frame.displayHeight

				const ctx = this.#canvas.getContext("2d")
				if (!ctx) throw new Error("failed to get canvas context")

				ctx.drawImage(frame, 0, 0, frame.displayWidth, frame.displayHeight) // TODO respect aspect ratio
				frame.close()
			})
		}
	}

	#start(controller: TransformStreamDefaultController<VideoFrame>) {
		this.#decoder = new VideoDecoder({
			output: (frame: VideoFrame) => {
				controller.enqueue(frame)
			},
			error: console.error,
		})

		this.#decoder.configure({
			codec: this.#track.codec,
			codedHeight: this.#track.resolution.height,
			codedWidth: this.#track.resolution.width,
			description: this.#track.description,
			optimizeForLatency: true,
		})
	}

	#transform(frame: Frame) {
		const chunk = new EncodedVideoChunk({
			type: frame.type,
			data: frame.data,
			timestamp: frame.timestamp,
		})

		this.#decoder.decode(chunk)
	}
}
