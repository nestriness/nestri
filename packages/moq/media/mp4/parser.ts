import * as MP4 from "./index"

export interface Frame {
	track: MP4.Track // The track this frame belongs to
	sample: MP4.Sample // The actual sample contain the frame data
}

// Decode a MP4 container into individual samples.
export class Parser {
	info!: MP4.Info

	#mp4 = MP4.New()
	#offset = 0

	#samples: Array<Frame> = []

	constructor(init: Uint8Array) {
		this.#mp4.onError = (err) => {
			console.error("MP4 error", err)
		}

		this.#mp4.onReady = (info: MP4.Info) => {
			this.info = info

			// Extract all of the tracks, because we don't know if it's audio or video.
			for (const track of info.tracks) {
				this.#mp4.setExtractionOptions(track.id, track, { nbSamples: 1 })
			}
		}

		this.#mp4.onSamples = (_track_id: number, track: MP4.Track, samples: MP4.Sample[]) => {
			for (const sample of samples) {
				this.#samples.push({ track, sample })
			}
		}

		this.#mp4.start()

		// For some reason we need to modify the underlying ArrayBuffer with offset
		const copy = new Uint8Array(init)
		const buffer = copy.buffer as MP4.ArrayBuffer
		buffer.fileStart = this.#offset

		this.#mp4.appendBuffer(buffer)
		this.#offset += buffer.byteLength
		this.#mp4.flush()

		if (!this.info) {
			throw new Error("could not parse MP4 info")
		}
	}

	decode(chunk: Uint8Array): Array<Frame> {
		const copy = new Uint8Array(chunk)

		// For some reason we need to modify the underlying ArrayBuffer with offset
		const buffer = copy.buffer as MP4.ArrayBuffer
		buffer.fileStart = this.#offset

		// Parse the data
		this.#mp4.appendBuffer(buffer)
		this.#mp4.flush()

		this.#offset += buffer.byteLength

		const samples = [...this.#samples]
		this.#samples.length = 0

		return samples
	}
}
