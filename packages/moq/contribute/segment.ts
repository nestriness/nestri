import { Frame } from "../karp/frame"

export class Segment {
	id: number

	// Take in a stream of frames
	input: WritableStream<Frame>

	// Output a stream of bytes, which we fork for each new subscriber.
	#cache: ReadableStream<Uint8Array>

	timestamp = 0

	constructor(id: number) {
		this.id = id

		// Set a max size for each segment, dropping the tail if it gets too long.
		// We tee the reader, so this limit applies to the FASTEST reader.
		const backpressure = new ByteLengthQueuingStrategy({ highWaterMark: 8_000_000 })

		const transport = new TransformStream<Frame, Uint8Array>(
			{
				transform: (frame: Frame, controller) => {
					// Compute the max timestamp of the segment
					this.timestamp = Math.max(this.timestamp, frame.timestamp)

					// Push the chunk to any listeners.
					controller.enqueue(frame.data)
				},
			},
			undefined,
			backpressure,
		)

		this.input = transport.writable
		this.#cache = transport.readable
	}

	// Split the output reader into two parts.
	chunks(): ReadableStream<Uint8Array> {
		const [tee, cache] = this.#cache.tee()
		this.#cache = cache
		return tee
	}
}
