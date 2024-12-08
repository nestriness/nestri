import { asError } from "../common/error"
import * as Message from "./message"
import { Reader, Stream } from "./stream"

import type { Queue } from "../common/async"
import { Closed } from "./error"
import type { Track, TrackReader } from "./model"
import { Publisher } from "./publisher"
import { type Announced, Subscriber } from "./subscriber"

export class Connection {
	// The established WebTransport session.
	#quic: WebTransport

	// Use to receive/send session messages.
	#session: Stream

	// Module for contributing tracks.
	#publisher: Publisher

	// Module for distributing tracks.
	#subscriber: Subscriber

	// Async work running in the background
	#running: Promise<void>

	constructor(quic: WebTransport, session: Stream) {
		this.#quic = quic
		this.#session = session

		this.#publisher = new Publisher(this.#quic)
		this.#subscriber = new Subscriber(this.#quic)

		this.#running = this.#run()
	}

	close(code = 0, reason = "") {
		this.#quic.close({ closeCode: code, reason })
	}

	async #run(): Promise<void> {
		const session = this.#runSession().catch((err) => new Error("failed to run session: ", err))
		const bidis = this.#runBidis().catch((err) => new Error("failed to run bidis: ", err))
		const unis = this.#runUnis().catch((err) => new Error("failed to run unis: ", err))

		await Promise.all([session, bidis, unis])
	}

	publish(track: TrackReader) {
		this.#publisher.publish(track)
	}

	async announced(prefix: string[] = []): Promise<Queue<Announced>> {
		return this.#subscriber.announced(prefix)
	}

	async subscribe(track: Track): Promise<TrackReader> {
		return await this.#subscriber.subscribe(track)
	}

	async #runSession() {
		// Receive messages until the connection is closed.
		for (;;) {
			const msg = await Message.SessionInfo.decode_maybe(this.#session.reader)
			if (!msg) break
			// TODO use the session info
		}
	}

	async #runBidis() {
		for (;;) {
			const next = await Stream.accept(this.#quic)
			if (!next) {
				break
			}

			const [msg, stream] = next
			this.#runBidi(msg, stream).catch((err) => stream.writer.reset(Closed.extract(err)))
		}
	}

	async #runBidi(msg: Message.Bi, stream: Stream) {
		console.debug("received bi stream: ", msg)

		if (msg instanceof Message.SessionClient) {
			throw new Error("duplicate session stream")
		}

		if (msg instanceof Message.AnnounceInterest) {
			if (!this.#subscriber) {
				throw new Error("not a subscriber")
			}

			return await this.#publisher.runAnnounce(msg, stream)
		}
		if (msg instanceof Message.Subscribe) {
			if (!this.#publisher) {
				throw new Error("not a publisher")
			}

			return await this.#publisher.runSubscribe(msg, stream)
		}
		if (msg instanceof Message.Datagrams) {
			if (!this.#publisher) {
				throw new Error("not a publisher")
			}

			return await this.#publisher.runDatagrams(msg, stream)
		}
		if (msg instanceof Message.Fetch) {
			if (!this.#publisher) {
				throw new Error("not a publisher")
			}

			return await this.#publisher.runFetch(msg, stream)
		}
		if (msg instanceof Message.InfoRequest) {
			if (!this.#publisher) {
				throw new Error("not a publisher")
			}

			return await this.#publisher.runInfo(msg, stream)
		}
	}

	async #runUnis() {
		for (;;) {
			const next = await Reader.accept(this.#quic)
			if (!next) {
				break
			}

			const [msg, stream] = next
			this.#runUni(msg, stream).catch((err) => stream.stop(Closed.extract(err)))
		}
	}

	async #runUni(msg: Message.Uni, stream: Reader) {
		console.debug("received uni stream: ", msg)

		if (msg instanceof Message.Group) {
			if (!this.#subscriber) {
				throw new Error("not a subscriber")
			}

			return this.#subscriber.runGroup(msg, stream)
		}
	}

	async closed(): Promise<Error> {
		try {
			await this.#running
			return new Error("closed")
		} catch (e) {
			return asError(e)
		}
	}
}
