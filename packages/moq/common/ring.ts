// Ring buffer with audio samples.

enum STATE {
	READ_POS = 0, // The current read position
	WRITE_POS, // The current write position
	LENGTH, // Clever way of saving the total number of enums values.
}

interface FrameCopyToOptions {
	frameCount?: number
	frameOffset?: number
	planeIndex: number
}

// This is implemented by AudioData in WebCodecs, but we don't import it because it's a DOM type.
interface Frame {
	numberOfFrames: number
	numberOfChannels: number
	copyTo(dst: Float32Array, options: FrameCopyToOptions): void
}

// No prototype to make this easier to send via postMessage
export class RingShared {
	state: SharedArrayBuffer

	channels: SharedArrayBuffer[]
	capacity: number

	constructor(channels: number, capacity: number) {
		// Store the current state in a separate ring buffer.
		this.state = new SharedArrayBuffer(STATE.LENGTH * Int32Array.BYTES_PER_ELEMENT)

		// Create a buffer for each audio channel
		this.channels = []
		for (let i = 0; i < channels; i += 1) {
			const buffer = new SharedArrayBuffer(capacity * Float32Array.BYTES_PER_ELEMENT)
			this.channels.push(buffer)
		}

		this.capacity = capacity
	}
}

export class Ring {
	state: Int32Array
	channels: Float32Array[]
	capacity: number

	constructor(shared: RingShared) {
		this.state = new Int32Array(shared.state)

		this.channels = []
		for (const channel of shared.channels) {
			this.channels.push(new Float32Array(channel))
		}

		this.capacity = shared.capacity
	}

	// Write samples for single audio frame, returning the total number written.
	write(frame: Frame): number {
		const readPos = Atomics.load(this.state, STATE.READ_POS)
		const writePos = Atomics.load(this.state, STATE.WRITE_POS)

		const startPos = writePos
		let endPos = writePos + frame.numberOfFrames

		if (endPos > readPos + this.capacity) {
			endPos = readPos + this.capacity
			if (endPos <= startPos) {
				// No space to write
				return 0
			}
		}

		const startIndex = startPos % this.capacity
		const endIndex = endPos % this.capacity

		// Loop over each channel
		for (let i = 0; i < this.channels.length; i += 1) {
			const channel = this.channels[i]

			// If the AudioData doesn't have enough channels, duplicate it.
			const planeIndex = Math.min(i, frame.numberOfChannels - 1)

			if (startIndex < endIndex) {
				// One continuous range to copy.
				const full = channel.subarray(startIndex, endIndex)

				frame.copyTo(full, {
					planeIndex,
					frameCount: endIndex - startIndex,
				})
			} else {
				const first = channel.subarray(startIndex)
				const second = channel.subarray(0, endIndex)

				frame.copyTo(first, {
					planeIndex,
					frameCount: first.length,
				})

				// We need this conditional when startIndex == 0 and endIndex == 0
				// When capacity=4410 and frameCount=1024, this was happening 52s into the audio.
				if (second.length) {
					frame.copyTo(second, {
						planeIndex,
						frameOffset: first.length,
						frameCount: second.length,
					})
				}
			}
		}

		Atomics.store(this.state, STATE.WRITE_POS, endPos)

		return endPos - startPos
	}

	read(dst: Float32Array[]): number {
		const readPos = Atomics.load(this.state, STATE.READ_POS)
		const writePos = Atomics.load(this.state, STATE.WRITE_POS)

		const startPos = readPos
		let endPos = startPos + dst[0].length

		if (endPos > writePos) {
			endPos = writePos
			if (endPos <= startPos) {
				// Nothing to read
				return 0
			}
		}

		const startIndex = startPos % this.capacity
		const endIndex = endPos % this.capacity

		// Loop over each channel
		for (let i = 0; i < dst.length; i += 1) {
			if (i >= this.channels.length) {
				// ignore excess channels
			}

			const input = this.channels[i]
			const output = dst[i]

			if (startIndex < endIndex) {
				const full = input.subarray(startIndex, endIndex)
				output.set(full)
			} else {
				const first = input.subarray(startIndex)
				const second = input.subarray(0, endIndex)

				output.set(first)
				output.set(second, first.length)
			}
		}

		Atomics.store(this.state, STATE.READ_POS, endPos)

		return endPos - startPos
	}

	clear() {
		const pos = Atomics.load(this.state, STATE.WRITE_POS)
		Atomics.store(this.state, STATE.READ_POS, pos)
	}

	size() {
		// TODO is this thread safe?
		const readPos = Atomics.load(this.state, STATE.READ_POS)
		const writePos = Atomics.load(this.state, STATE.WRITE_POS)

		return writePos - readPos
	}
}
