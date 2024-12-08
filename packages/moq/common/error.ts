// I hate javascript
export function asError(e: unknown): Error {
	if (e instanceof Error) {
		return e
	}
	if (typeof e === "string") {
		return new Error(e)
	}
	return new Error(String(e))
}

export function isError(e: unknown): e is Error {
	return e instanceof Error
}
