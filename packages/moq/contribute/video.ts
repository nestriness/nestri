import { Deferred } from "../common/async"
import type { Frame } from "../karp/frame"
import type { Group, Track } from "../transfork"
import { Closed } from "../transfork/error"

const SUPPORTED = [
	"avc1", // H.264
	"hev1", // HEVC (aka h.265)
	// "av01", // TDOO support AV1
]

export interface EncoderSupported {
	codecs: string[]
}

export class Packer {
	#source: MediaStreamTrackProcessor<VideoFrame>
	#encoder: Encoder

	#data: Track
	#current?: Group

	constructor(track: MediaStreamVideoTrack, encoder: Encoder, data: Track) {
		this.#source = new MediaStreamTrackProcessor({ track })
		this.#encoder = encoder
		this.#data = data
	}

	async run() {
		const output = new WritableStream({
			write: (chunk) => this.#write(chunk),
			close: () => this.#close(),
			abort: (e) => this.#close(e),
		})

		return this.#source.readable.pipeThrough(this.#encoder.frames).pipeTo(output)
	}

	#write(frame: Frame) {
		if (!this.#current || frame.type === "key") {
			if (this.#current) {
				this.#current.close()
			}

			this.#current = this.#data.appendGroup()
		}

		frame.encode(this.#current)
	}

	#close(err?: unknown) {
		const closed = Closed.from(err)
		if (this.#current) {
			this.#current.close(closed)
		}

		this.#data.close(closed)
	}
}

export class Encoder {
	#encoder!: VideoEncoder
	#encoderConfig: VideoEncoderConfig
	#decoderConfig = new Deferred<VideoDecoderConfig>()

	// true if we should insert a keyframe, undefined when the encoder should decide
	#keyframeNext: true | undefined = true

	// Count the number of frames without a keyframe.
	#keyframeCounter = 0

	// Converts raw rames to encoded frames.
	frames: TransformStream<VideoFrame, EncodedVideoChunk>

	constructor(config: VideoEncoderConfig) {
		config.bitrateMode ??= "constant"
		config.latencyMode ??= "realtime"

		this.#encoderConfig = config

		this.frames = new TransformStream({
			start: this.#start.bind(this),
			transform: this.#transform.bind(this),
			flush: this.#flush.bind(this),
		})
	}

	static async isSupported(config: VideoEncoderConfig) {
		// Check if we support a specific codec family
		const short = config.codec.substring(0, 4)
		if (!SUPPORTED.includes(short)) return false

		// Default to hardware encoding
		config.hardwareAcceleration ??= "prefer-hardware"

		// Default to CBR
		config.bitrateMode ??= "constant"

		// Default to realtime encoding
		config.latencyMode ??= "realtime"

		const res = await VideoEncoder.isConfigSupported(config)
		return !!res.supported
	}

	async decoderConfig(): Promise<VideoDecoderConfig> {
		return await this.#decoderConfig.promise
	}

	#start(controller: TransformStreamDefaultController<EncodedVideoChunk>) {
		this.#encoder = new VideoEncoder({
			output: (frame, metadata) => {
				this.#enqueue(controller, frame, metadata)
			},
			error: (err) => {
				this.#decoderConfig.reject(err)
				throw err
			},
		})

		this.#encoder.configure(this.#encoderConfig)
	}

	#transform(frame: VideoFrame) {
		const encoder = this.#encoder

		// Set keyFrame to undefined when we're not sure so the encoder can decide.
		encoder.encode(frame, { keyFrame: this.#keyframeNext })
		this.#keyframeNext = undefined

		frame.close()
	}

	#enqueue(
		controller: TransformStreamDefaultController<EncodedVideoChunk>,
		frame: EncodedVideoChunk,
		metadata?: EncodedVideoChunkMetadata,
	) {
		if (this.#decoderConfig.pending) {
			const config = metadata?.decoderConfig
			if (!config) throw new Error("missing decoder config")
			this.#decoderConfig.resolve(config)
		}

		if (frame.type === "key") {
			this.#keyframeCounter = 0
		} else {
			this.#keyframeCounter += 1
			const framesPerGop = this.#encoderConfig.framerate ? 2 * this.#encoderConfig.framerate : 60
			if (this.#keyframeCounter + this.#encoder.encodeQueueSize >= framesPerGop) {
				this.#keyframeNext = true
			}
		}

		controller.enqueue(frame)
	}

	#flush() {
		this.#encoder.close()
	}

	get config() {
		return this.#encoderConfig
	}
}
