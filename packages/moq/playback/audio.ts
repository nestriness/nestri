/// <reference types="vite/client" />

import { Ring, RingShared } from "../common/ring"
import { Component } from "./timeline"
import * as Catalog from "../karp/catalog"
import { Frame } from "../karp/frame"

// This is a non-standard way of importing worklet/workers.
// Unfortunately, it's the only option because of a Vite bug: https://github.com/vitejs/vite/issues/11823
import workletURL from "./worklet/index.ts?worker&url"

export class Renderer {
	#context: AudioContext
	#worklet: Promise<AudioWorkletNode>

	#ring: Ring
	#timeline: Component
	#track: Catalog.Audio

	#decoder!: AudioDecoder
	#stream: TransformStream<Frame, AudioData>

	constructor(track: Catalog.Audio, timeline: Component) {
		this.#track = track
		this.#context = new AudioContext({
			latencyHint: "interactive",
			sampleRate: track.sample_rate,
		})

		this.#worklet = this.load(track)

		this.#timeline = timeline
		const ring = new RingShared(2, track.sample_rate / 20) // 50ms
		this.#ring = new Ring(ring)

		this.#stream = new TransformStream({
			start: this.#start.bind(this),
			transform: this.#transform.bind(this),
		})

		this.#run().catch((err) => console.error("failed to run audio renderer: ", err))
	}

	private async load(config: Catalog.Audio): Promise<AudioWorkletNode> {
		// Load the worklet source code.
		await this.#context.audioWorklet.addModule(workletURL)

		const volume = this.#context.createGain()
		volume.gain.value = 2.0

		// Create the worklet
		const worklet = new AudioWorkletNode(this.#context, "renderer")

		worklet.port.addEventListener("message", this.on.bind(this))
		worklet.onprocessorerror = (e: Event) => {
			console.error("Audio worklet error:", e)
		}

		// Connect the worklet to the volume node and then to the speakers
		worklet.connect(volume)
		volume.connect(this.#context.destination)

		worklet.port.postMessage({ config })

		return worklet
	}

	private on(_event: MessageEvent) {
		// TODO
	}

	play() {
		this.#context.resume().catch((err) => console.warn("failed to resume audio context: ", err))
	}

	close() {
		this.#context.close().catch((err) => console.warn("failed to close audio context: ", err))
	}

	#start(controller: TransformStreamDefaultController) {
		this.#decoder = new AudioDecoder({
			output: (frame: AudioData) => {
				controller.enqueue(frame)
			},
			error: console.warn,
		})

		// We only support OPUS right now which doesn't need a description.
		this.#decoder.configure({
			codec: this.#track.codec,
			sampleRate: this.#track.sample_rate,
			numberOfChannels: this.#track.channel_count,
		})
	}

	#transform(frame: Frame) {
		const chunk = new EncodedAudioChunk({
			type: frame.type,
			timestamp: frame.timestamp,
			data: frame.data,
		})

		this.#decoder.decode(chunk)
	}

	async #run() {
		const reader = this.#timeline.frames.pipeThrough(this.#stream).getReader()

		for (;;) {
			const { value: frame, done } = await reader.read()
			if (done) break

			// Write audio samples to the ring buffer, dropping when there's no space.
			const written = this.#ring.write(frame)

			if (written < frame.numberOfFrames) {
				console.warn(`droppped ${frame.numberOfFrames - written} audio samples`)
			}
		}
	}
}
