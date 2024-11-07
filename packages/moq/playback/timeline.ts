import { Frame } from "../karp/frame"

export interface Range {
	start: number
	end: number
}

export class Timeline {
	// Maintain audio and video seprarately
	audio: Component
	video: Component

	// Construct a timeline
	constructor() {
		this.audio = new Component()
		this.video = new Component()
	}
}

interface Segment {
	sequence: number
	frames: ReadableStream<Frame>
}

export class Component {
	#current?: Segment

	frames: ReadableStream<Frame>
	#segments: TransformStream<Segment, Segment>

	constructor() {
		this.frames = new ReadableStream({
			pull: this.#pull.bind(this),
			cancel: this.#cancel.bind(this),
		})

		// This is a hack to have an async channel with 100 items.
		this.#segments = new TransformStream({}, { highWaterMark: 100 })
	}

	get segments() {
		return this.#segments.writable
	}

	async #pull(controller: ReadableStreamDefaultController<Frame>) {
		for (;;) {
			// Get the next segment to render.
			const segments = this.#segments.readable.getReader()

			let res
			if (this.#current) {
				// Get the next frame to render.
				const frames = this.#current.frames.getReader()

				// Wait for either the frames or segments to be ready.
				// NOTE: This assume that the first promise gets priority.
				res = await Promise.race([frames.read(), segments.read()])

				frames.releaseLock()
			} else {
				res = await segments.read()
			}

			segments.releaseLock()

			const { value, done } = res

			if (done) {
				// We assume the current segment has been closed
				// TODO support the segments stream closing
				this.#current = undefined
				continue
			}

			if (!isSegment(value)) {
				// Return so the reader can decide when to get the next frame.
				controller.enqueue(value)
				return
			}

			// We didn't get any frames, and instead got a new segment.
			if (this.#current) {
				if (value.sequence < this.#current.sequence) {
					// Our segment is older than the current, abandon it.
					await value.frames.cancel("skipping segment; too old")
					continue
				} else {
					// Our segment is newer than the current, cancel the old one.
					await this.#current.frames.cancel("skipping segment; too slow")
				}
			}

			this.#current = value
		}
	}

	async #cancel(reason: any) {
		if (this.#current) {
			await this.#current.frames.cancel(reason)
		}

		const segments = this.#segments.readable.getReader()
		for (;;) {
			const { value: segment, done } = await segments.read()
			if (done) break

			await segment.frames.cancel(reason)
		}
	}
}

// Return if a type is a segment or frame
// eslint-disable-next-line @typescript-eslint/no-redundant-type-constituents
function isSegment(value: Segment | Frame): value is Segment {
	// eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
	return (value as Segment).frames !== undefined
}
