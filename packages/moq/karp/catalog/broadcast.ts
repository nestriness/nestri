import * as Transfork from "../../transfork"
import { type Audio, decodeAudio } from "./audio"
import { type Video, decodeVideo } from "./video"

export interface Broadcast {
	path: string[]
	video: Video[]
	audio: Audio[]
}

export function encode(catalog: Broadcast): Uint8Array {
	const encoder = new TextEncoder()
	console.debug("encoding catalog", catalog)
	const str = JSON.stringify(catalog)
	return encoder.encode(str)
}

export function decode(path: string[], raw: Uint8Array): Broadcast {
	const decoder = new TextDecoder()
	const str = decoder.decode(raw)

	const catalog = JSON.parse(str)
	if (!decodeBroadcast(catalog)) {
		console.error("invalid catalog", catalog)
		throw new Error("invalid catalog")
	}

	catalog.path = path
	return catalog
}

export async function fetch(connection: Transfork.Connection, path: string[]): Promise<Broadcast> {
	const track = new Transfork.Track(path.concat("catalog.json"), 0)
	const sub = await connection.subscribe(track)
	try {
		const segment = await sub.nextGroup()
		if (!segment) throw new Error("no catalog data")

		const frame = await segment.readFrame()
		if (!frame) throw new Error("no catalog frame")

		segment.close()
		return decode(path, frame)
	} finally {
		sub.close()
	}
}

export function decodeBroadcast(o: unknown): o is Broadcast {
	if (typeof o !== "object" || o === null) return false

	const catalog = o as Partial<Broadcast>
	if (catalog.audio === undefined) catalog.audio = []
	if (!Array.isArray(catalog.audio)) return false
	if (!catalog.audio.every((track: unknown) => decodeAudio(track))) return false

	if (catalog.video === undefined) catalog.video = []
	if (!Array.isArray(catalog.video)) return false
	if (!catalog.video.every((track: unknown) => decodeVideo(track))) return false

	return true
}
