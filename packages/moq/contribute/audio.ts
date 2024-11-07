import { Deferred } from "../common/async"
import { Frame } from "../karp/frame"
import { Group, Track } from "../transfork"
import { Closed } from "../transfork/error"

const SUPPORTED = [
	// TODO support AAC
	// "mp4a"
	"Opus",
]

export class Packer {
	#source: MediaStreamTrackProcessor<AudioData>
	#encoder: Encoder

	#data: Track
	#current?: Group

	constructor(track: MediaStreamAudioTrack, encoder: Encoder, data: Track) {
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
		// TODO use a fixed interval instead of keyframes (audio)
		// TODO actually just align with video
		if (!this.#current || frame.type === "key") {
			if (this.#current) {
				this.#current.close()
			}

			this.#current = this.#data.appendGroup()
		}

		this.#current.writeFrame(frame.data)
	}

	#close(err?: any) {
		const closed = Closed.from(err)
		if (this.#current) {
			this.#current.close(closed)
		}

		this.#data.close(closed)
	}
}

export class Encoder {
	#encoder!: AudioEncoder
	#encoderConfig: AudioEncoderConfig
	#decoderConfig = new Deferred<AudioDecoderConfig>()

	frames: TransformStream<AudioData, EncodedAudioChunk>

	constructor(config: AudioEncoderConfig) {
		this.#encoderConfig = config

		this.frames = new TransformStream({
			start: this.#start.bind(this),
			transform: this.#transform.bind(this),
			flush: this.#flush.bind(this),
		})
	}

	#start(controller: TransformStreamDefaultController<EncodedAudioChunk>) {
		this.#encoder = new AudioEncoder({
			output: (frame, metadata) => {
				this.#enqueue(controller, frame, metadata)
			},
			error: (err) => {
				throw err
			},
		})

		this.#encoder.configure(this.#encoderConfig)
	}

	#transform(frame: AudioData) {
		this.#encoder.encode(frame)
		frame.close()
	}

	#enqueue(
		controller: TransformStreamDefaultController<EncodedAudioChunk>,
		frame: EncodedAudioChunk,
		metadata?: EncodedAudioChunkMetadata,
	) {
		const config = metadata?.decoderConfig
		if (config && !this.#decoderConfig.pending) {
			const config = metadata.decoderConfig
			if (!config) throw new Error("missing decoder config")

			this.#decoderConfig.resolve(config)
		}

		controller.enqueue(frame)
	}

	#flush() {
		this.#encoder.close()
	}

	static async isSupported(config: AudioEncoderConfig) {
		// Check if we support a specific codec family
		const short = config.codec.substring(0, 4)
		if (!SUPPORTED.includes(short)) return false

		const res = await AudioEncoder.isConfigSupported(config)
		return !!res.supported
	}

	get config() {
		return this.#encoderConfig
	}

	async decoderConfig(): Promise<AudioDecoderConfig> {
		return await this.#decoderConfig.promise
	}
}
