import type { Reader, Writer } from "./stream"

export enum Version {
	DRAFT_00 = 0xff000000,
	DRAFT_01 = 0xff000001,
	DRAFT_02 = 0xff000002,
	DRAFT_03 = 0xff000003,
	FORK_00 = 0xff0bad00,
	FORK_01 = 0xff0bad01,
	FORK_02 = 0xff0bad02,
}

export class Extensions {
	entries: Map<bigint, Uint8Array>

	constructor() {
		this.entries = new Map()
	}

	set(id: bigint, value: Uint8Array) {
		this.entries.set(id, value)
	}

	get(id: bigint): Uint8Array | undefined {
		return this.entries.get(id)
	}

	remove(id: bigint): Uint8Array | undefined {
		const value = this.entries.get(id)
		this.entries.delete(id)
		return value
	}

	async encode(w: Writer) {
		await w.u53(this.entries.size)
		for (const [id, value] of this.entries) {
			await w.u62(id)
			await w.u53(value.length)
			await w.write(value)
		}
	}

	static async decode(r: Reader): Promise<Extensions> {
		const count = await r.u53()
		const params = new Extensions()

		for (let i = 0; i < count; i++) {
			const id = await r.u62()
			const size = await r.u53()
			const value = await r.read(size)

			if (params.entries.has(id)) {
				throw new Error(`duplicate parameter id: ${id}`)
			}

			params.entries.set(id, value)
		}

		return params
	}
}

export enum Order {
	Any = 0,
	Ascending = 1,
	Descending = 2,
}

export class SessionClient {
	versions: Version[]
	extensions: Extensions

	static StreamID = 0x0

	constructor(versions: Version[], extensions = new Extensions()) {
		this.versions = versions
		this.extensions = extensions
	}

	async encode(w: Writer) {
		await w.u53(this.versions.length)
		for (const v of this.versions) {
			await w.u53(v)
		}

		await this.extensions.encode(w)
	}

	static async decode(r: Reader): Promise<SessionClient> {
		const versions = []
		const count = await r.u53()
		for (let i = 0; i < count; i++) {
			versions.push(await r.u53())
		}

		const extensions = await Extensions.decode(r)
		return new SessionClient(versions, extensions)
	}
}

export class SessionServer {
	version: Version
	extensions: Extensions

	constructor(version: Version, extensions = new Extensions()) {
		this.version = version
		this.extensions = extensions
	}

	async encode(w: Writer) {
		await w.u53(this.version)
		await this.extensions.encode(w)
	}

	static async decode(r: Reader): Promise<SessionServer> {
		const version = await r.u53()
		const extensions = await Extensions.decode(r)
		return new SessionServer(version, extensions)
	}
}

export class SessionInfo {
	bitrate: number

	constructor(bitrate: number) {
		this.bitrate = bitrate
	}

	async encode(w: Writer) {
		await w.u53(this.bitrate)
	}

	static async decode(r: Reader): Promise<SessionInfo> {
		const bitrate = await r.u53()
		return new SessionInfo(bitrate)
	}

	static async decode_maybe(r: Reader): Promise<SessionInfo | undefined> {
		if (await r.done()) return
		return await SessionInfo.decode(r)
	}
}

export type AnnounceStatus = "active" | "closed"

export class Announce {
	suffix: string[]
	status: AnnounceStatus

	constructor(suffix: string[], status: AnnounceStatus) {
		this.suffix = suffix
		this.status = status
	}

	async encode(w: Writer) {
		await w.u53(this.status === "active" ? 1 : 0)
		await w.path(this.suffix)
	}

	static async decode(r: Reader): Promise<Announce> {
		const status = (await r.u53()) === 1 ? "active" : "closed"
		const suffix = await r.path()
		return new Announce(suffix, status)
	}

	static async decode_maybe(r: Reader): Promise<Announce | undefined> {
		if (await r.done()) return
		return await Announce.decode(r)
	}
}

export class AnnounceInterest {
	static StreamID = 0x1

	constructor(public prefix: string[]) {}

	async encode(w: Writer) {
		await w.path(this.prefix)
	}

	static async decode(r: Reader): Promise<AnnounceInterest> {
		const prefix = await r.path()
		return new AnnounceInterest(prefix)
	}
}

export class SubscribeUpdate {
	priority: number
	order = Order.Any
	expires = 0 // ms

	start?: bigint
	end?: bigint

	constructor(priority: number) {
		this.priority = priority
	}

	async encode(w: Writer) {
		await w.u53(this.priority)
		await w.u53(this.order)
		await w.u53(this.expires)
		await w.u62(this.start ? this.start + 1n : 0n)
		await w.u62(this.end ? this.end + 1n : 0n)
	}

	static async decode(r: Reader): Promise<SubscribeUpdate> {
		const priority = await r.u53()
		const order = await r.u53()
		if (order > 2) {
			throw new Error(`invalid order: ${order}`)
		}

		const expires = await r.u53()
		const start = await r.u62()
		const end = await r.u62()

		const update = new SubscribeUpdate(priority)
		update.order = order
		update.expires = expires
		update.start = start === 0n ? undefined : start - 1n
		update.end = end === 0n ? undefined : end - 1n

		return update
	}

	static async decode_maybe(r: Reader): Promise<SubscribeUpdate | undefined> {
		if (await r.done()) return
		return await SubscribeUpdate.decode(r)
	}
}

export class Subscribe extends SubscribeUpdate {
	id: bigint
	path: string[]

	static StreamID = 0x2

	constructor(id: bigint, path: string[], priority: number) {
		super(priority)

		this.id = id
		this.path = path
	}

	async encode(w: Writer) {
		await w.u62(this.id)
		await w.path(this.path)
		await super.encode(w)
	}

	static async decode(r: Reader): Promise<Subscribe> {
		const id = await r.u62()
		const path = await r.path()
		const update = await SubscribeUpdate.decode(r)

		const subscribe = new Subscribe(id, path, update.priority)
		subscribe.order = update.order
		subscribe.expires = update.expires
		subscribe.start = update.start
		subscribe.end = update.end

		return subscribe
	}
}

export class Datagrams extends Subscribe {
	static StreamID = 0x3
}

export class Info {
	priority: number
	order = Order.Descending
	expires = 0
	latest?: number

	constructor(priority: number) {
		this.priority = priority
	}

	async encode(w: Writer) {
		await w.u53(this.priority)
		await w.u53(this.order)
		await w.u53(this.expires)
		await w.u53(this.latest ? this.latest + 1 : 0)
	}

	static async decode(r: Reader): Promise<Info> {
		const priority = await r.u53()
		const order = await r.u53()
		const latest = await r.u53()

		const info = new Info(priority)
		info.latest = latest === 0 ? undefined : latest - 1
		info.order = order

		return info
	}
}

export class InfoRequest {
	path: string[]

	static StreamID = 0x5

	constructor(path: string[]) {
		this.path = path
	}

	async encode(w: Writer) {
		await w.path(this.path)
	}

	static async decode(r: Reader): Promise<InfoRequest> {
		const path = await r.path()
		return new InfoRequest(path)
	}
}

export class FetchUpdate {
	priority: number

	constructor(priority: number) {
		this.priority = priority
	}

	async encode(w: Writer) {
		await w.u53(this.priority)
	}

	static async decode(r: Reader): Promise<FetchUpdate> {
		return new FetchUpdate(await r.u53())
	}

	static async decode_maybe(r: Reader): Promise<FetchUpdate | undefined> {
		if (await r.done()) return
		return await FetchUpdate.decode(r)
	}
}

export class Fetch extends FetchUpdate {
	path: string[]

	static StreamID = 0x4

	constructor(path: string[], priority: number) {
		super(priority)
		this.path = path
	}

	async encode(w: Writer) {
		await w.path(this.path)
		await super.encode(w)
	}

	static async decode(r: Reader): Promise<Fetch> {
		const path = await r.path()
		const update = await FetchUpdate.decode(r)

		const fetch = new Fetch(path, update.priority)
		return fetch
	}
}

export class Group {
	subscribe: bigint
	sequence: number

	static StreamID = 0x0

	constructor(subscribe: bigint, sequence: number) {
		this.subscribe = subscribe
		this.sequence = sequence
	}

	async encode(w: Writer) {
		await w.u62(this.subscribe)
		await w.u53(this.sequence)
	}

	static async decode(r: Reader): Promise<Group> {
		return new Group(await r.u62(), await r.u53())
	}
}

export class GroupDrop {
	sequence: number
	count: number
	error: number

	constructor(sequence: number, count: number, error: number) {
		this.sequence = sequence
		this.count = count
		this.error = error
	}

	async encode(w: Writer) {
		await w.u53(this.sequence)
		await w.u53(this.count)
		await w.u53(this.error)
	}

	static async decode(r: Reader): Promise<GroupDrop> {
		return new GroupDrop(await r.u53(), await r.u53(), await r.u53())
	}
}

export class Frame {
	payload: Uint8Array

	constructor(payload: Uint8Array) {
		this.payload = payload
	}

	async encode(w: Writer) {
		await w.u53(this.payload.byteLength)
		await w.write(this.payload)
	}

	static async decode(r: Reader): Promise<Frame> {
		const size = await r.u53()
		const payload = await r.read(size)
		return new Frame(payload)
	}
}

export type Bi = SessionClient | AnnounceInterest | Subscribe | Datagrams | Fetch | InfoRequest
export type Uni = Group
