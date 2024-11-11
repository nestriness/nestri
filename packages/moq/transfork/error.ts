export class Closed extends Error {
	readonly code?: number

	constructor(code?: number) {
		super(`closed code=${code}`)
		this.code = code
	}

	static from(err: unknown): Closed {
		return new Closed(Closed.extract(err))
	}

	static extract(err: unknown): number {
		if (err instanceof WebTransportError && err.streamErrorCode !== null) {
			return err.streamErrorCode
		}

		return 0
	}
}
