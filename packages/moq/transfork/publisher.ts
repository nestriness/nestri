import * as Message from "./message"
import { Watch } from "../common/async"
import { Stream, Writer } from "./stream"
import { Closed } from "./error"
import { GroupReader, TrackReader } from "./model"

export class Publisher {
	#quic: WebTransport

	// Our announced broadcasts.
	#announce = new Map<string[], TrackReader>()

	// Their subscribed tracks.
	#subscribe = new Map<bigint, Subscribed>()

	constructor(quic: WebTransport) {
		this.#quic = quic
	}

	// Publish a track
	publish(track: TrackReader) {
		if (this.#announce.has(track.path)) {
			throw new Error(`already announced: ${track.path.toString()}`)
		}

		this.#announce.set(track.path, track)

		// TODO: clean up announcements
		// track.closed().then(() => this.#announce.delete(track.path))
	}

	#get(path: string[]): TrackReader | undefined {
		return this.#announce.get(path)
	}

	async runAnnounce(msg: Message.AnnounceInterest, stream: Stream) {
		for (const announce of this.#announce.values()) {
			if (announce.path.length < msg.prefix.length) continue

			const prefix = announce.path.slice(0, msg.prefix.length)
			if (prefix != msg.prefix) continue

			const suffix = announce.path.slice(msg.prefix.length)

			const active = new Message.Announce(suffix, "active")
			await active.encode(stream.writer)
		}

		// TODO support updates.
		// Until then, just keep the stream open.
		await stream.reader.closed()
	}

	async runSubscribe(msg: Message.Subscribe, stream: Stream) {
		if (this.#subscribe.has(msg.id)) {
			throw new Error(`duplicate subscribe for id: ${msg.id}`)
		}

		const track = this.#get(msg.path)
		if (!track) {
			await stream.writer.reset(404)
			return
		}

		const subscribe = new Subscribed(msg, track, this.#quic)

		// TODO close the stream when done
		subscribe.run().catch((err) => console.warn("failed to run subscribe: ", err))

		try {
			const info = new Message.Info(track.priority)
			info.order = track.order
			info.latest = track.latest
			await info.encode(stream.writer)

			for (;;) {
				// TODO try_decode
				const update = await Message.SubscribeUpdate.decode_maybe(stream.reader)
				if (!update) {
					subscribe.close()
					break
				}

				// TODO use the update
			}
		} catch (err) {
			subscribe.close(Closed.from(err))
		}
	}

	async runDatagrams(msg: Message.Datagrams, stream: Stream) {
		await stream.writer.reset(501)
		throw new Error("datagrams not implemented")
	}

	async runFetch(msg: Message.Fetch, stream: Stream) {
		await stream.writer.reset(501)
		throw new Error("fetch not implemented")
	}

	async runInfo(msg: Message.InfoRequest, stream: Stream) {
		const track = this.#get(msg.path)
		if (!track) {
			await stream.writer.reset(404)
			return
		}

		const info = new Message.Info(track.priority)
		info.order = track.order
		info.latest = track.latest

		await info.encode(stream.writer)

		throw new Error("info not implemented")
	}
}

class Subscribed {
	#id: bigint
	#track: TrackReader
	#quic: WebTransport

	#closed = new Watch<Closed | undefined>(undefined)

	constructor(msg: Message.Subscribe, track: TrackReader, quic: WebTransport) {
		this.#id = msg.id
		this.#track = track
		this.#quic = quic
	}

	async run() {
		const closed = this.closed()

		for (;;) {
			const [group, done] = await Promise.all([this.#track.nextGroup(), closed])
			if (done) return
			if (!group) break

			this.#runGroup(group).catch((err) => console.warn("failed to run group: ", err))
		}

		// TODO wait until all groups are done
		this.close()
	}

	async #runGroup(group: GroupReader) {
		const msg = new Message.Group(this.#id, group.id)
		const stream = await Writer.open(this.#quic, msg)

		for (;;) {
			const frame = await group.readFrame()
			if (!frame) break

			await stream.u53(frame.byteLength)
			await stream.write(frame)
		}
	}

	close(err = new Closed()) {
		this.#closed.update(err)
		this.#track.close()
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
