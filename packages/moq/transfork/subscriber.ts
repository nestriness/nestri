import { Queue, Watch } from "../common/async"
import { Closed } from "./error"
import { FrameReader } from "./frame"
import * as Message from "./message"
import type { Track, TrackReader } from "./model"
import { type Reader, Stream } from "./stream"

export class Subscriber {
	#quic: WebTransport

	// Our subscribed tracks.
	#subscribe = new Map<bigint, Subscribe>()
	#subscribeNext = 0n

	constructor(quic: WebTransport) {
		this.#quic = quic
	}

	async announced(prefix: string[]): Promise<Queue<Announced>> {
		const announced = new Queue<Announced>()

		const msg = new Message.AnnounceInterest(prefix)
		const stream = await Stream.open(this.#quic, msg)

		this.runAnnounced(stream, announced, prefix)
			.then(() => announced.close())
			.catch((err) => announced.abort(err))

		return announced
	}

	async runAnnounced(stream: Stream, announced: Queue<Announced>, prefix: string[]) {
		const toggle: Map<string[], Announced> = new Map()

		try {
			for (;;) {
				const announce = await Message.Announce.decode_maybe(stream.reader)
				if (!announce) {
					break
				}

				const existing = toggle.get(announce.suffix)
				if (existing) {
					if (announce.status === "active") {
						throw new Error("duplicate announce")
					}

					existing.close()
					toggle.delete(announce.suffix)
				} else {
					if (announce.status === "closed") {
						throw new Error("unknown announce")
					}

					const path = prefix.concat(announce.suffix)
					const item = new Announced(path)
					await announced.push(item)
					toggle.set(announce.suffix, item)
				}
			}
		} finally {
			for (const item of toggle.values()) {
				item.close()
			}
		}
	}

	// TODO: Deduplicate identical subscribes
	async subscribe(track: Track): Promise<TrackReader> {
		const id = this.#subscribeNext++
		const msg = new Message.Subscribe(id, track.path, track.priority)

		const stream = await Stream.open(this.#quic, msg)
		const subscribe = new Subscribe(id, stream, track)

		this.#subscribe.set(subscribe.id, subscribe)

		try {
			const ok = await Message.Info.decode(stream.reader)

			/*
			for (;;) {
				const dropped = await Message.GroupDrop.decode(stream.reader)
				console.debug("dropped", dropped)
			}
				*/

			return subscribe.track.reader()
		} catch (err) {
			console.error(err)
			this.#subscribe.delete(subscribe.id)
			await subscribe.close(Closed.from(err))
			throw err
		}
	}

	async runGroup(group: Message.Group, stream: Reader) {
		const subscribe = this.#subscribe.get(group.subscribe)
		if (!subscribe) return

		const writer = subscribe.track.createGroup(group.sequence)

		const reader = new FrameReader(stream)
		for (;;) {
			const frame = await reader.read()
			if (!frame) break

			writer.writeFrame(frame)
		}

		writer.close()
	}
}

export class Announced {
	readonly path: string[]

	#closed = new Watch<Closed | undefined>(undefined)

	constructor(path: string[]) {
		this.path = path
	}

	close(err = new Closed()) {
		this.#closed.update(err)
	}

	async closed(): Promise<Closed> {
		let [closed, next] = this.#closed.value()
		for (;;) {
			if (closed !== undefined) return closed
			if (!next) return new Closed()
			;[closed, next] = await next
		}
	}
}

export class Subscribe {
	readonly id: bigint
	readonly track: Track
	readonly stream: Stream

	// A queue of received streams for this subscription.
	#closed = new Watch<Closed | undefined>(undefined)

	constructor(id: bigint, stream: Stream, track: Track) {
		this.id = id
		this.track = track
		this.stream = stream
	}

	async run() {
		try {
			await this.closed()
			await this.close()
		} catch (err) {
			await this.close(Closed.from(err))
		}
	}

	async close(closed?: Closed) {
		this.track.close(closed)
		await this.stream.close(closed?.code)
	}

	async closed() {
		await this.stream.reader.closed()
	}
}
