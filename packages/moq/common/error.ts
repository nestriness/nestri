// I hate javascript
export function asError(e: any): Error {
	if (e instanceof Error) {
		return e
	} else if (typeof e === "string") {
		return new Error(e)
	} else {
		return new Error(String(e))
	}
}

export function isError(e: any): e is Error {
	return e instanceof Error
}
