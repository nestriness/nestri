export type GroupOrder = "desc" | "asc"

export interface Track {
	name: string
	priority: number
}

export function decodeTrack(track: any): track is Track {
	if (typeof track.name !== "string") return false
	if (typeof track.priority !== "number") return false
	return true
}
