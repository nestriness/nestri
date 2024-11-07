import * as Hex from "../../common/hex"
import { decodeTrack, Track } from "./track"

export interface Video {
	track: Track
	codec: string
	description?: Uint8Array
	bitrate?: number
	frame_rate?: number
	resolution: Dimensions
}

export interface Dimensions {
	width: number
	height: number
}

export function decodeVideo(o: any): o is Video {
	if (!decodeTrack(o.track)) return false
	if (typeof o.codec !== "string") return false
	if (typeof o.description !== "string") return false

	o.description = Hex.decode(o.description)

	return true
}
