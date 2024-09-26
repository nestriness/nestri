import { Reader, Writer } from "./stream"

export type Message = Client | Server
export type Role = "publisher" | "subscriber" | "both"

export enum Version {
	DRAFT_00 = 0xff000000,
	DRAFT_01 = 0xff000001,
	DRAFT_02 = 0xff000002,
	DRAFT_03 = 0xff000003,
	DRAFT_04 = 0xff000004,
	KIXEL_00 = 0xbad00,
	KIXEL_01 = 0xbad01,
}

// NOTE: These are forked from moq-transport-00.
//   1. messages lack a sized length
//   2. parameters are not optional and written in order (role + path)
//   3. role indicates local support only, not remote support

export interface Client {
	versions: Version[]
	role: Role
	params?: Parameters
}

export interface Server {
	version: Version
	params?: Parameters
}

export class Stream {
	recv: Decoder
	send: Encoder

	constructor(r: Reader, w: Writer) {
		this.recv = new Decoder(r)
		this.send = new Encoder(w)
	}
}

export type Parameters = Map<bigint, Uint8Array>

export class Decoder {
	r: Reader

	constructor(r: Reader) {
		this.r = r
	}

	async client(): Promise<Client> {
		const type = await this.r.u53()
		if (type !== 0x40) throw new Error(`client SETUP type must be 0x40, got ${type}`)

		const count = await this.r.u53()

		const versions = []
		for (let i = 0; i < count; i++) {
			const version = await this.r.u53()
			versions.push(version)
		}

		const params = await this.parameters()
		const role = this.role(params?.get(0n))

		return {
			versions,
			role,
			params,
		}
	}

	async server(): Promise<Server> {
		const type = await this.r.u53()
		if (type !== 0x41) throw new Error(`server SETUP type must be 0x41, got ${type}`)

		const version = await this.r.u53()
		const params = await this.parameters()

		return {
			version,
			params,
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

	role(raw: Uint8Array | undefined): Role {
		if (!raw) throw new Error("missing role parameter")
		if (raw.length != 1) throw new Error("multi-byte varint not supported")

		switch (raw[0]) {
			case 1:
				return "publisher"
			case 2:
				return "subscriber"
			case 3:
				return "both"
			default:
				throw new Error(`invalid role: ${raw[0]}`)
		}
	}
}

export class Encoder {
	w: Writer

	constructor(w: Writer) {
		this.w = w
	}

	async client(c: Client) {
		await this.w.u53(0x40)
		await this.w.u53(c.versions.length)
		for (const v of c.versions) {
			await this.w.u53(v)
		}

		// I hate it
		const params = c.params ?? new Map()
		params.set(0n, new Uint8Array([c.role == "publisher" ? 1 : c.role == "subscriber" ? 2 : 3]))
		await this.parameters(params)
	}

	async server(s: Server) {
		await this.w.u53(0x41)
		await this.w.u53(s.version)
		await this.parameters(s.params)
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
}
