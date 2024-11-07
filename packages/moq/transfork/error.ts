export class Closed extends Error {
	readonly code?: number

	constructor(code?: number) {
		super(`closed code=${code}`)
		this.code = code
	}

	static from(err: any): Closed {
		return new Closed(this.extract(err))
	}

	static extract(err: any): number {
		if (err instanceof WebTransportError && err.streamErrorCode !== null) {
			return err.streamErrorCode
		}

		return 0
	}
}
