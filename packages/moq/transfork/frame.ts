import type { Reader, Writer } from "./stream"

export class FrameReader {
	#stream: Reader

	constructor(stream: Reader) {
		this.#stream = stream
	}

	// Returns the next frame
	async read(): Promise<Uint8Array | undefined> {
		if (await this.#stream.done()) return

		const size = await this.#stream.u53()
		const payload = await this.#stream.read(size)

		return payload
	}

	async stop(code: number) {
		await this.#stream.stop(code)
	}
}

export class FrameWriter {
	#stream: Writer

	constructor(stream: Writer) {
		this.#stream = stream
	}

	// Writes the next frame
	async write(payload: Uint8Array) {
		await this.#stream.u53(payload.byteLength)
		await this.#stream.write(payload)
	}

	async close() {
		await this.#stream.close()
	}

	async reset(code: number) {
		await this.#stream.reset(code)
	}
}
