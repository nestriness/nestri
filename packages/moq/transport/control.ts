import { Reader, Writer } from "./stream"

export type Message = Subscriber | Publisher

// Sent by subscriber
export type Subscriber = Subscribe | Unsubscribe | AnnounceOk | AnnounceError

export function isSubscriber(m: Message): m is Subscriber {
	return (
		m.kind == Msg.Subscribe || m.kind == Msg.Unsubscribe || m.kind == Msg.AnnounceOk || m.kind == Msg.AnnounceError
	)
}

// Sent by publisher
export type Publisher = SubscribeOk | SubscribeError | SubscribeDone | Announce | Unannounce

export function isPublisher(m: Message): m is Publisher {
	return (
		m.kind == Msg.SubscribeOk ||
		m.kind == Msg.SubscribeError ||
		m.kind == Msg.SubscribeDone ||
		m.kind == Msg.Announce ||
		m.kind == Msg.Unannounce
	)
}

// I wish we didn't have to split Msg and Id into separate enums.
// However using the string in the message makes it easier to debug.
// We'll take the tiny performance hit until I'm better at Typescript.
export enum Msg {
	// NOTE: object and setup are in other modules
	Subscribe = "subscribe",
	SubscribeOk = "subscribe_ok",
	SubscribeError = "subscribe_error",
	SubscribeDone = "subscribe_done",
	Unsubscribe = "unsubscribe",
	Announce = "announce",
	AnnounceOk = "announce_ok",
	AnnounceError = "announce_error",
	Unannounce = "unannounce",
	GoAway = "go_away",
}

enum Id {
	// NOTE: object and setup are in other modules
	// Object = 0,
	// Setup = 1,

	Subscribe = 0x3,
	SubscribeOk = 0x4,
	SubscribeError = 0x5,
	SubscribeDone = 0xb,
	Unsubscribe = 0xa,
	Announce = 0x6,
	AnnounceOk = 0x7,
	AnnounceError = 0x8,
	Unannounce = 0x9,
	GoAway = 0x10,
}

export interface Subscribe {
	kind: Msg.Subscribe

	id: bigint
	trackId: bigint
	namespace: string
	name: string

	location: Location

	params?: Parameters
}

export type Location = LatestGroup | LatestObject | AbsoluteStart | AbsoluteRange

export interface LatestGroup {
	mode: "latest_group"
}

export interface LatestObject {
	mode: "latest_object"
}

export interface AbsoluteStart {
	mode: "absolute_start"
	start_group: number
	start_object: number
}

export interface AbsoluteRange {
	mode: "absolute_range"
	start_group: number
	start_object: number
	end_group: number
	end_object: number
}

export type Parameters = Map<bigint, Uint8Array>

export interface SubscribeOk {
	kind: Msg.SubscribeOk
	id: bigint
	expires: bigint
	latest?: [number, number]
}

export interface SubscribeDone {
	kind: Msg.SubscribeDone
	id: bigint
	code: bigint
	reason: string
	final?: [number, number]
}

export interface SubscribeError {
	kind: Msg.SubscribeError
	id: bigint
	code: bigint
	reason: string
}

export interface Unsubscribe {
	kind: Msg.Unsubscribe
	id: bigint
}

export interface Announce {
	kind: Msg.Announce
	namespace: string
	params?: Parameters
}

export interface AnnounceOk {
	kind: Msg.AnnounceOk
	namespace: string
}

export interface AnnounceError {
	kind: Msg.AnnounceError
	namespace: string
	code: bigint
	reason: string
}

export interface Unannounce {
	kind: Msg.Unannounce
	namespace: string
}

export class Stream {
	private decoder: Decoder
	private encoder: Encoder

	#mutex = Promise.resolve()

	constructor(r: Reader, w: Writer) {
		this.decoder = new Decoder(r)
		this.encoder = new Encoder(w)
	}

	// Will error if two messages are read at once.
	async recv(): Promise<Message> {
		const msg = await this.decoder.message()
		console.log("received message", msg)
		return msg
	}

	async send(msg: Message) {
		const unlock = await this.#lock()
		try {
			console.log("sending message", msg)
			await this.encoder.message(msg)
		} finally {
			unlock()
		}
	}

	async #lock() {
		// Make a new promise that we can resolve later.
		let done: () => void
		const p = new Promise<void>((resolve) => {
			done = () => resolve()
		})

		// Wait until the previous lock is done, then resolve our our lock.
		const lock = this.#mutex.then(() => done)

		// Save our lock as the next lock.
		this.#mutex = p

		// Return the lock.
		return lock
	}
}

export class Decoder {
	r: Reader

	constructor(r: Reader) {
		this.r = r
	}

	private async msg(): Promise<Msg> {
		const t = await this.r.u53()
		switch (t) {
			case Id.Subscribe:
				return Msg.Subscribe
			case Id.SubscribeOk:
				return Msg.SubscribeOk
			case Id.SubscribeDone:
				return Msg.SubscribeDone
			case Id.SubscribeError:
				return Msg.SubscribeError
			case Id.Unsubscribe:
				return Msg.Unsubscribe
			case Id.Announce:
				return Msg.Announce
			case Id.AnnounceOk:
				return Msg.AnnounceOk
			case Id.AnnounceError:
				return Msg.AnnounceError
			case Id.Unannounce:
				return Msg.Unannounce
			case Id.GoAway:
				return Msg.GoAway
		}

		throw new Error(`unknown control message type: ${t}`)
	}

	async message(): Promise<Message> {
		const t = await this.msg()
		switch (t) {
			case Msg.Subscribe:
				return this.subscribe()
			case Msg.SubscribeOk:
				return this.subscribe_ok()
			case Msg.SubscribeError:
				return this.subscribe_error()
			case Msg.SubscribeDone:
				return this.subscribe_done()
			case Msg.Unsubscribe:
				return this.unsubscribe()
			case Msg.Announce:
				return this.announce()
			case Msg.AnnounceOk:
				return this.announce_ok()
			case Msg.Unannounce:
				return this.unannounce()
			case Msg.AnnounceError:
				return this.announce_error()
			case Msg.GoAway:
				throw new Error("TODO: implement go away")
		}
	}

	private async subscribe(): Promise<Subscribe> {
		return {
			kind: Msg.Subscribe,
			id: await this.r.u62(),
			trackId: await this.r.u62(),
			namespace: await this.r.string(),
			name: await this.r.string(),
			location: await this.location(),
			params: await this.parameters(),
		}
	}

	private async location(): Promise<Location> {
		const mode = await this.r.u62()
		if (mode == 1n) {
			return {
				mode: "latest_group",
			}
		} else if (mode == 2n) {
			return {
				mode: "latest_object",
			}
		} else if (mode == 3n) {
			return {
				mode: "absolute_start",
				start_group: await this.r.u53(),
				start_object: await this.r.u53(),
			}
		} else if (mode == 4n) {
			return {
				mode: "absolute_range",
				start_group: await this.r.u53(),
				start_object: await this.r.u53(),
				end_group: await this.r.u53(),
				end_object: await this.r.u53(),
			}
		} else {
			throw new Error(`invalid filter type: ${mode}`)
		}
	}

	private async parameters(): Promise<Parameters | undefined> {
		const count = await this.r.u53()
		if (count == 0) return undefined

		const params = new Map<bigint, Uint8Array>()

		for (let i = 0; i < count; i++) {
			const id = await this.r.u62()
			const size = await this.r.u53()
			const value = await this.r.read(size)

			if (params.has(id)) {
				throw new Error(`duplicate parameter id: ${id}`)
			}

			params.set(id, value)
		}

		return params
	}

	private async subscribe_ok(): Promise<SubscribeOk> {
		const id = await this.r.u62()
		const expires = await this.r.u62()

		let latest: [number, number] | undefined

		const flag = await this.r.u8()
		if (flag === 1) {
			latest = [await this.r.u53(), await this.r.u53()]
		} else if (flag !== 0) {
			throw new Error(`invalid final flag: ${flag}`)
		}

		return {
			kind: Msg.SubscribeOk,
			id,
			expires,
			latest,
		}
	}

	private async subscribe_done(): Promise<SubscribeDone> {
		const id = await this.r.u62()
		const code = await this.r.u62()
		const reason = await this.r.string()

		let final: [number, number] | undefined

		const flag = await this.r.u8()
		if (flag === 1) {
			final = [await this.r.u53(), await this.r.u53()]
		} else if (flag !== 0) {
			throw new Error(`invalid final flag: ${flag}`)
		}

		return {
			kind: Msg.SubscribeDone,
			id,
			code,
			reason,
			final,
		}
	}

	private async subscribe_error(): Promise<SubscribeError> {
		return {
			kind: Msg.SubscribeError,
			id: await this.r.u62(),
			code: await this.r.u62(),
			reason: await this.r.string(),
		}
	}

	private async unsubscribe(): Promise<Unsubscribe> {
		return {
			kind: Msg.Unsubscribe,
			id: await this.r.u62(),
		}
	}

	private async announce(): Promise<Announce> {
		const namespace = await this.r.string()

		return {
			kind: Msg.Announce,
			namespace,
			params: await this.parameters(),
		}
	}

	private async announce_ok(): Promise<AnnounceOk> {
		return {
			kind: Msg.AnnounceOk,
			namespace: await this.r.string(),
		}
	}

	private async announce_error(): Promise<AnnounceError> {
		return {
			kind: Msg.AnnounceError,
			namespace: await this.r.string(),
			code: await this.r.u62(),
			reason: await this.r.string(),
		}
	}

	private async unannounce(): Promise<Unannounce> {
		return {
			kind: Msg.Unannounce,
			namespace: await this.r.string(),
		}
	}
}

export class Encoder {
	w: Writer

	constructor(w: Writer) {
		this.w = w
	}

	async message(m: Message) {
		switch (m.kind) {
			case Msg.Subscribe:
				return this.subscribe(m)
			case Msg.SubscribeOk:
				return this.subscribe_ok(m)
			case Msg.SubscribeError:
				return this.subscribe_error(m)
			case Msg.SubscribeDone:
				return this.subscribe_done(m)
			case Msg.Unsubscribe:
				return this.unsubscribe(m)
			case Msg.Announce:
				return this.announce(m)
			case Msg.AnnounceOk:
				return this.announce_ok(m)
			case Msg.AnnounceError:
				return this.announce_error(m)
			case Msg.Unannounce:
				return this.unannounce(m)
		}
	}

	async subscribe(s: Subscribe) {
		await this.w.u53(Id.Subscribe)
		await this.w.u62(s.id)
		await this.w.u62(s.trackId)
		await this.w.string(s.namespace)
		await this.w.string(s.name)
		await this.location(s.location)
		await this.parameters(s.params)
	}

	private async location(l: Location) {
		switch (l.mode) {
			case "latest_group":
				await this.w.u62(1n)
				break
			case "latest_object":
				await this.w.u62(2n)
				break
			case "absolute_start":
				await this.w.u62(3n)
				await this.w.u53(l.start_group)
				await this.w.u53(l.start_object)
				break
			case "absolute_range":
				await this.w.u62(3n)
				await this.w.u53(l.start_group)
				await this.w.u53(l.start_object)
				await this.w.u53(l.end_group)
				await this.w.u53(l.end_object)
		}
	}

	private async parameters(p: Parameters | undefined) {
		if (!p) {
			await this.w.u8(0)
			return
		}

		await this.w.u53(p.size)
		for (const [id, value] of p) {
			await this.w.u62(id)
			await this.w.u53(value.length)
			await this.w.write(value)
		}
	}

	async subscribe_ok(s: SubscribeOk) {
		await this.w.u53(Id.SubscribeOk)
		await this.w.u62(s.id)
		await this.w.u62(s.expires)

		if (s.latest !== undefined) {
			await this.w.u8(1)
			await this.w.u53(s.latest[0])
			await this.w.u53(s.latest[1])
		} else {
			await this.w.u8(0)
		}
	}

	async subscribe_done(s: SubscribeDone) {
		await this.w.u53(Id.SubscribeDone)
		await this.w.u62(s.id)
		await this.w.u62(s.code)
		await this.w.string(s.reason)

		if (s.final !== undefined) {
			await this.w.u8(1)
			await this.w.u53(s.final[0])
			await this.w.u53(s.final[1])
		} else {
			await this.w.u8(0)
		}
	}

	async subscribe_error(s: SubscribeError) {
		await this.w.u53(Id.SubscribeError)
		await this.w.u62(s.id)
	}

	async unsubscribe(s: Unsubscribe) {
		await this.w.u53(Id.Unsubscribe)
		await this.w.u62(s.id)
	}

	async announce(a: Announce) {
		await this.w.u53(Id.Announce)
		await this.w.string(a.namespace)
		await this.w.u53(0) // parameters
	}

	async announce_ok(a: AnnounceOk) {
		await this.w.u53(Id.AnnounceOk)
		await this.w.string(a.namespace)
	}

	async announce_error(a: AnnounceError) {
		await this.w.u53(Id.AnnounceError)
		await this.w.string(a.namespace)
		await this.w.u62(a.code)
		await this.w.string(a.reason)
	}

	async unannounce(a: Unannounce) {
		await this.w.u53(Id.Unannounce)
		await this.w.string(a.namespace)
	}
}
