/// <reference types="vite/client" />

import * as Message from "./worker/message"

// This is a non-standard way of importing worklet/workers.
// Unfortunately, it's the only option because of a Vite bug: https://github.com/vitejs/vite/issues/11823
import workletURL from "./worklet/index.ts?worker&url"

// NOTE: This must be on the main thread
export class Audio {
	context: AudioContext
	worklet: Promise<AudioWorkletNode>

	constructor(config: Message.ConfigAudio) {
		this.context = new AudioContext({
			latencyHint: "interactive",
			sampleRate: config.sampleRate,
		})

		this.worklet = this.load(config)
	}

	private async load(config: Message.ConfigAudio): Promise<AudioWorkletNode> {
		// Load the worklet source code.
		await this.context.audioWorklet.addModule(workletURL)

		const volume = this.context.createGain()
		volume.gain.value = 2.0

		// Create the worklet
		const worklet = new AudioWorkletNode(this.context, "renderer")

		worklet.port.addEventListener("message", this.on.bind(this))
		worklet.onprocessorerror = (e: Event) => {
			console.error("Audio worklet error:", e)
		}

		// Connect the worklet to the volume node and then to the speakers
		worklet.connect(volume)
		volume.connect(this.context.destination)

		worklet.port.postMessage({ config })

		return worklet
	}

	private on(_event: MessageEvent) {
		// TODO
	}
}
