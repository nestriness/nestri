import { Reader, Writer } from "./stream"
export { Reader, Writer }

export enum StreamType {
	Object = 0x0,
	Track = 0x50,
	Group = 0x51,
}

export enum Status {
	OBJECT_NULL = 1,
	GROUP_NULL = 2,
	GROUP_END = 3,
	TRACK_END = 4,
}

export interface TrackHeader {
	type: StreamType.Track
	sub: bigint
	track: bigint
	priority: number // VarInt with a u32 maximum value
}

export interface TrackChunk {
	group: number // The group sequence, as a number because 2^53 is enough.
	object: number
	payload: Uint8Array | Status
}

export interface GroupHeader {
	type: StreamType.Group
	sub: bigint
	track: bigint
	group: number // The group sequence, as a number because 2^53 is enough.
	priority: number // VarInt with a u32 maximum value
}

export interface GroupChunk {
	object: number
	payload: Uint8Array | Status
}

export interface ObjectHeader {
	type: StreamType.Object
	sub: bigint
	track: bigint
	group: number
	object: number
	priority: number
	status: number
}

export interface ObjectChunk {
	payload: Uint8Array
}

type WriterType<T> = T extends TrackHeader
	? TrackWriter
	: T extends GroupHeader
	? GroupWriter
	: T extends ObjectHeader
	? ObjectWriter
	: never

export class Objects {
	private quic: WebTransport

	constructor(quic: WebTransport) {
		this.quic = quic
	}

	async send<T extends TrackHeader | GroupHeader | ObjectHeader>(h: T): Promise<WriterType<T>> {
		const stream = await this.quic.createUnidirectionalStream()
		const w = new Writer(stream)

		await w.u53(h.type)
		await w.u62(h.sub)
		await w.u62(h.track)

		let res: WriterType<T>

		if (h.type == StreamType.Object) {
			await w.u53(h.group)
			await w.u53(h.object)
			await w.u53(h.priority)
			await w.u53(h.status)

			res = new ObjectWriter(h, w) as WriterType<T>
		} else if (h.type === StreamType.Group) {
			await w.u53(h.group)
			await w.u53(h.priority)

			res = new GroupWriter(h, w) as WriterType<T>
		} else if (h.type === StreamType.Track) {
			await w.u53(h.priority)

			res = new TrackWriter(h, w) as WriterType<T>
		} else {
			throw new Error("unknown header type")
		}

		// console.trace("send object", res.header)

		return res
	}

	async recv(): Promise<TrackReader | GroupReader | ObjectReader | undefined> {
		const streams = this.quic.incomingUnidirectionalStreams.getReader()

		const { value, done } = await streams.read()
		streams.releaseLock()

		if (done) return

		const r = new Reader(new Uint8Array(), value)
		const type = (await r.u53()) as StreamType
		let res: TrackReader | GroupReader | ObjectReader

		if (type == StreamType.Track) {
			const h: TrackHeader = {
				type,
				sub: await r.u62(),
				track: await r.u62(),
				priority: await r.u53(),
			}

			res = new TrackReader(h, r)
		} else if (type == StreamType.Group) {
			const h: GroupHeader = {
				type,
				sub: await r.u62(),
				track: await r.u62(),
				group: await r.u53(),
				priority: await r.u53(),
			}
			res = new GroupReader(h, r)
		} else if (type == StreamType.Object) {
			const h = {
				type,
				sub: await r.u62(),
				track: await r.u62(),
				group: await r.u53(),
				object: await r.u53(),
				status: await r.u53(),
				priority: await r.u53(),
			}

			res = new ObjectReader(h, r)
		} else {
			throw new Error("unknown stream type")
		}

		// console.trace("receive object", res.header)

		return res
	}
}

export class TrackWriter {
	constructor(
		public header: TrackHeader,
		public stream: Writer,
	) {}

	async write(c: TrackChunk) {
		await this.stream.u53(c.group)
		await this.stream.u53(c.object)

		if (c.payload instanceof Uint8Array) {
			await this.stream.u53(c.payload.byteLength)
			await this.stream.write(c.payload)
		} else {
			// empty payload with status
			await this.stream.u53(0)
			await this.stream.u53(c.payload as number)
		}
	}

	async close() {
		await this.stream.close()
	}
}

export class GroupWriter {
	constructor(
		public header: GroupHeader,
		public stream: Writer,
	) {}

	async write(c: GroupChunk) {
		await this.stream.u53(c.object)
		if (c.payload instanceof Uint8Array) {
			await this.stream.u53(c.payload.byteLength)
			await this.stream.write(c.payload)
		} else {
			await this.stream.u53(0)
			await this.stream.u53(c.payload as number)
		}
	}

	async close() {
		await this.stream.close()
	}
}

export class ObjectWriter {
	constructor(
		public header: ObjectHeader,
		public stream: Writer,
	) {}

	async write(c: ObjectChunk) {
		await this.stream.write(c.payload)
	}

	async close() {
		await this.stream.close()
	}
}

export class TrackReader {
	constructor(
		public header: TrackHeader,
		public stream: Reader,
	) {}

	async read(): Promise<TrackChunk | undefined> {
		if (await this.stream.done()) {
			return
		}

		const group = await this.stream.u53()
		const object = await this.stream.u53()
		const size = await this.stream.u53()

		let payload
		if (size == 0) {
			payload = (await this.stream.u53()) as Status
		} else {
			payload = await this.stream.read(size)
		}

		return {
			group,
			object,
			payload,
		}
	}

	async close() {
		await this.stream.close()
	}
}

export class GroupReader {
	constructor(
		public header: GroupHeader,
		public stream: Reader,
	) {}

	async read(): Promise<GroupChunk | undefined> {
		if (await this.stream.done()) {
			return
		}

		const object = await this.stream.u53()
		const size = await this.stream.u53()

		let payload
		if (size == 0) {
			payload = (await this.stream.u53()) as Status
		} else {
			payload = await this.stream.read(size)
		}

		return {
			object,
			payload,
		}
	}

	async close() {
		await this.stream.close()
	}
}

export class ObjectReader {
	constructor(
		public header: ObjectHeader,
		public stream: Reader,
	) {}

	// NOTE: Can only be called once.
	async read(): Promise<ObjectChunk | undefined> {
		if (await this.stream.done()) {
			return
		}

		return {
			payload: await this.stream.readAll(),
		}
	}

	async close() {
		await this.stream.close()
	}
}
