// TODO add support for @/ to avoid relative imports
import { Ring } from "../../common/ring"
import type * as Message from "./message"

class Renderer extends AudioWorkletProcessor {
	ring?: Ring
	base: number

	constructor() {
		// The super constructor call is required.
		super()

		this.base = 0
		this.port.onmessage = this.onMessage.bind(this)
	}

	onMessage(e: MessageEvent) {
		const msg = e.data as Message.From
		if (msg.config) {
			this.onConfig(msg.config)
		}
	}

	onConfig(config: Message.Config) {
		this.ring = new Ring(config.ring)
	}

	// Inputs and outputs in groups of 128 samples.
	process(_inputs: Float32Array[][], outputs: Float32Array[][], _parameters: Record<string, Float32Array>): boolean {
		if (!this.ring) {
			// Paused
			return true
		}

		if (outputs.length !== 1) {
			throw new Error("only a single track is supported")
		}

		if (this.ring.size() === this.ring.capacity) {
			// This is a hack to clear any latency in the ring buffer.
			// The proper solution is to play back slightly faster?
			console.warn("resyncing ring buffer")
			this.ring.clear()
			return true
		}

		const output = outputs[0]

		const size = this.ring.read(output)
		if (size < output.length) {
			// TODO trigger rebuffering event
		}

		return true
	}
}

registerProcessor("renderer", Renderer)
