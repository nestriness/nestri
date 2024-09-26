// Rename some stuff so it's on brand.
// We need a separate file so this file can use the rename too.
import * as MP4 from "./rename"
export * from "./rename"

export * from "./parser"

export function isAudioTrack(track: MP4.Track): track is MP4.AudioTrack {
	// eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
	return (track as MP4.AudioTrack).audio !== undefined
}

export function isVideoTrack(track: MP4.Track): track is MP4.VideoTrack {
	// eslint-disable-next-line @typescript-eslint/no-unnecessary-condition
	return (track as MP4.VideoTrack).video !== undefined
}

// TODO contribute to mp4box
MP4.BoxParser.dOpsBox.prototype.write = function (stream: MP4.Stream) {
	this.size = this.ChannelMappingFamily === 0 ? 11 : 13 + this.ChannelMapping!.length
	this.writeHeader(stream)

	stream.writeUint8(this.Version)
	stream.writeUint8(this.OutputChannelCount)
	stream.writeUint16(this.PreSkip)
	stream.writeUint32(this.InputSampleRate)
	stream.writeInt16(this.OutputGain)
	stream.writeUint8(this.ChannelMappingFamily)

	if (this.ChannelMappingFamily !== 0) {
		stream.writeUint8(this.StreamCount!)
		stream.writeUint8(this.CoupledCount!)
		for (const mapping of this.ChannelMapping!) {
			stream.writeUint8(mapping)
		}
	}
}
