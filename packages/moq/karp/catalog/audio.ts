import { type Track, decodeTrack } from "./track"

export interface Audio {
	track: Track
	codec: string
	sample_rate: number
	channel_count: number
	bitrate?: number
}

export function decodeAudio(o: unknown): o is Audio {
	if (typeof o !== "object" || o === null) return false

	const obj = o as Partial<Audio>
	if (!decodeTrack(obj.track)) return false
	if (typeof obj.codec !== "string") return false
	if (typeof obj.sample_rate !== "number") return false
	if (typeof obj.channel_count !== "number") return false
	return true
}
