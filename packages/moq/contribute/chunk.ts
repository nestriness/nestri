// Extends EncodedVideoChunk, allowing a new "init" type
export interface Chunk {
	type: "init" | "key" | "delta"
	timestamp: number // microseconds
	duration: number // microseconds
	data: Uint8Array
}
