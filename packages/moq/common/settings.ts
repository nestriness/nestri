// MediaTrackSettings can represent both audio and video, which means a LOT of possibly undefined properties.
// This is a fork of the MediaTrackSettings interface with properties required for audio or vidfeo.
export interface AudioTrackSettings {
	deviceId: string
	groupId: string

	autoGainControl: boolean
	channelCount: number
	echoCancellation: boolean
	noiseSuppression: boolean
	sampleRate: number
	sampleSize: number
}

export interface VideoTrackSettings {
	deviceId: string
	groupId: string

	aspectRatio: number
	facingMode: "user" | "environment" | "left" | "right"
	frameRate: number
	height: number
	resizeMode: "none" | "crop-and-scale"
	width: number
}

export function isAudioTrackSettings(settings: MediaTrackSettings): settings is AudioTrackSettings {
	return "sampleRate" in settings
}

export function isVideoTrackSettings(settings: MediaTrackSettings): settings is VideoTrackSettings {
	return "width" in settings
}
