export type GroupOrder = "desc" | "asc"

export interface Track {
	name: string
	priority: number
}

export function decodeTrack(o: unknown): o is Track {
	if (typeof o !== "object" || o === null) return false

	const obj = o as Partial<Track>
	if (typeof obj.name !== "string") return false
	if (typeof obj.priority !== "number") return false
	return true
}
