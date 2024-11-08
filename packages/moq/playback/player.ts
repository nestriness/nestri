import { Connection } from "../transfork/connection"
import * as Catalog from "../karp/catalog"
import { Broadcast } from "./broadcast"

export interface PlayerConfig {
	connection: Connection
	path: string[]
	canvas: HTMLCanvasElement
}

// This class must be created on the main thread due to AudioContext.
export class Player {
	#config: PlayerConfig
	#running: Promise<void>

	constructor(config: PlayerConfig) {
		this.#config = config
		this.#running = this.#run()
	}

	async #run() {
		const announced = await this.#config.connection.announced(this.#config.path)

		let active = undefined
		let activeId = -1

		for (;;) {
			const announce = await announced.next()
			if (!announce) break

			if (announce.path.length == this.#config.path.length) {
				throw new Error("expected resumable broadcast")
			}

			const path = announce.path.slice(0, this.#config.path.length + 1)

			const id = parseInt(path[path.length - 1])
			if (id <= activeId) continue

			const catalog = await Catalog.fetch(this.#config.connection, path)

			active?.close()
			active = new Broadcast(this.#config.connection, catalog, this.#config.canvas)
			activeId = id
		}

		active?.close()
	}

	close() {
		this.#config.connection.close()
	}

	async closed() {
		await Promise.any([this.#running, this.#config.connection.closed()])
	}
}
