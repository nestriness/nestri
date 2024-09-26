import * as Control from "./control"
import { Queue, Watch } from "../common/async"
import { Objects, GroupWriter, ObjectWriter, StreamType, TrackWriter } from "./objects"

export class Publisher {
	// Used to send control messages
	#control: Control.Stream

	// Use to send objects.
	#objects: Objects

	// Our announced tracks.
	#announce = new Map<string, AnnounceSend>()

	// Their subscribed tracks.
	#subscribe = new Map<bigint, SubscribeRecv>()
	#subscribeQueue = new Queue<SubscribeRecv>(Number.MAX_SAFE_INTEGER) // Unbounded queue in case there's no receiver

	constructor(control: Control.Stream, objects: Objects) {
		this.#control = control
		this.#objects = objects
	}

	// Announce a track namespace.
	async announce(namespace: string): Promise<AnnounceSend> {
		if (this.#announce.has(namespace)) {
			throw new Error(`already announce: ${namespace}`)
		}

		const announce = new AnnounceSend(this.#control, namespace)
		this.#announce.set(namespace, announce)

		await this.#control.send({
			kind: Control.Msg.Announce,
			namespace,
		})

		return announce
	}

	// Receive the next new subscription
	async subscribed() {
		return await this.#subscribeQueue.next()
	}

	async recv(msg: Control.Subscriber) {
		if (msg.kind == Control.Msg.Subscribe) {
			await this.recvSubscribe(msg)
		} else if (msg.kind == Control.Msg.Unsubscribe) {
			this.recvUnsubscribe(msg)
		} else if (msg.kind == Control.Msg.AnnounceOk) {
			this.recvAnnounceOk(msg)
		} else if (msg.kind == Control.Msg.AnnounceError) {
			this.recvAnnounceError(msg)
		} else {
			throw new Error(`unknown control message`) // impossible
		}
	}

	recvAnnounceOk(msg: Control.AnnounceOk) {
		const announce = this.#announce.get(msg.namespace)
		if (!announce) {
			throw new Error(`announce OK for unknown announce: ${msg.namespace}`)
		}

		announce.onOk()
	}

	recvAnnounceError(msg: Control.AnnounceError) {
		const announce = this.#announce.get(msg.namespace)
		if (!announce) {
			// TODO debug this
			console.warn(`announce error for unknown announce: ${msg.namespace}`)
			return
		}

		announce.onError(msg.code, msg.reason)
	}

	async recvSubscribe(msg: Control.Subscribe) {
		if (this.#subscribe.has(msg.id)) {
			throw new Error(`duplicate subscribe for id: ${msg.id}`)
		}

		const subscribe = new SubscribeRecv(this.#control, this.#objects, msg)
		this.#subscribe.set(msg.id, subscribe)
		await this.#subscribeQueue.push(subscribe)

		await this.#control.send({ kind: Control.Msg.SubscribeOk, id: msg.id, expires: 0n })
	}

	recvUnsubscribe(_msg: Control.Unsubscribe) {
		throw new Error("TODO unsubscribe")
	}
}

export class AnnounceSend {
	#control: Control.Stream

	readonly namespace: string

	// The current state, updated by control messages.
	#state = new Watch<"init" | "ack" | Error>("init")

	constructor(control: Control.Stream, namespace: string) {
		this.#control = control
		this.namespace = namespace
	}

	async ok() {
		for (;;) {
			const [state, next] = this.#state.value()
			if (state === "ack") return
			if (state instanceof Error) throw state
			if (!next) throw new Error("closed")

			await next
		}
	}

	async active() {
		for (;;) {
			const [state, next] = this.#state.value()
			if (state instanceof Error) throw state
			if (!next) return

			await next
		}
	}

	async close() {
		// TODO implement unsubscribe
		// await this.#inner.sendUnsubscribe()
	}

	closed() {
		const [state, next] = this.#state.value()
		return state instanceof Error || next == undefined
	}

	onOk() {
		if (this.closed()) return
		this.#state.update("ack")
	}

	onError(code: bigint, reason: string) {
		if (this.closed()) return

		const err = new Error(`ANNOUNCE_ERROR (${code})` + reason ? `: ${reason}` : "")
		this.#state.update(err)
	}
}

export class SubscribeRecv {
	#control: Control.Stream
	#objects: Objects
	#id: bigint
	#trackId: bigint

	readonly namespace: string
	readonly track: string

	// The current state of the subscription.
	#state: "init" | "ack" | "closed" = "init"

	constructor(control: Control.Stream, objects: Objects, msg: Control.Subscribe) {
		this.#control = control // so we can send messages
		this.#objects = objects // so we can send objects
		this.#id = msg.id
		this.#trackId = msg.trackId
		this.namespace = msg.namespace
		this.track = msg.name
	}

	// Acknowledge the subscription as valid.
	async ack() {
		if (this.#state !== "init") return
		this.#state = "ack"

		// Send the control message.
		return this.#control.send({ kind: Control.Msg.SubscribeOk, id: this.#id, expires: 0n })
	}

	// Close the subscription with an error.
	async close(code = 0n, reason = "") {
		if (this.#state === "closed") return
		this.#state = "closed"

		return this.#control.send({
			kind: Control.Msg.SubscribeDone,
			id: this.#id,
			code,
			reason,
		})
	}

	// Create a writable data stream for the entire track
	async serve(props?: { priority: number }): Promise<TrackWriter> {
		return this.#objects.send({
			type: StreamType.Track,
			sub: this.#id,
			track: this.#trackId,
			priority: props?.priority ?? 0,
		})
	}

	// Create a writable data stream for a group within the track
	async group(props: { group: number; priority?: number }): Promise<GroupWriter> {
		return this.#objects.send({
			type: StreamType.Group,
			sub: this.#id,
			track: this.#trackId,
			group: props.group,
			priority: props.priority ?? 0,
		})
	}

	// Create a writable data stream for a single object within the track
	async object(props: { group: number; object: number; priority?: number }): Promise<ObjectWriter> {
		return this.#objects.send({
			type: StreamType.Object,
			sub: this.#id,
			track: this.#trackId,
			group: props.group,
			object: props.object,
			priority: props.priority ?? 0,
			status: 0,
		})
	}
}
