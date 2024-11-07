import { decodeTrack, Track } from "./track"

export interface Audio {
	track: Track
	codec: string
	sample_rate: number
	channel_count: number
	bitrate?: number
}

export function decodeAudio(o: any): o is Audio {
	if (!decodeTrack(o.track)) return false
	if (typeof o.codec !== "string") return false
	if (typeof o.sample_rate !== "number") return false
	if (typeof o.channel_count !== "number") return false
	return true
}
