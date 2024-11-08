// import type { Connection, SubscribeRecv } from "@nestri/libmoq/transport"
// import { asError } from "@nestri/moq/common/error"
// import { Client } from "@nestri/moq/transport/client"
// import * as Catalog from "@nestri/moq/media/catalog"
// import { type GroupWriter } from "@nestri/moq/transport/objects"

// export interface BroadcastConfig {
//     namespace: string
//     connection: Connection
// }
// export interface BroadcasterConfig {
//     url: string
//     namespace: string
//     fingerprint?: string // URL to fetch TLS certificate fingerprint
// }

// export interface BroadcastConfigTrack {
//     input: string
//     bitrate: number
// }

// export class Broadcast {
//     stream: GroupWriter | null
//     subscriber: SubscribeRecv | null
//     subscribed: boolean;


//     readonly config: BroadcastConfig
//     readonly catalog: Catalog.Root
//     readonly connection: Connection
//     readonly namespace: string

//     #running: Promise<void>

//     constructor(config: BroadcastConfig) {
//         this.subscribed = false
//         this.namespace = config.namespace
//         this.connection = config.connection
//         this.config = config
//         //Arbitrary values, just to keep TypeScript happy :)
//         this.catalog = {
//             version: 1,
//             streamingFormat: 1,
//             streamingFormatVersion: "0.2",
//             supportsDeltaUpdates: false,
//             commonTrackFields: {
//                 packaging: "loc",
//                 renderGroup: 1,
//             },
//             tracks: [{
//                 name: "tester",
//                 namespace: "tester",
//                 selectionParams: {}
//             }],
//         }
//         this.stream = null
//         this.subscriber = null

//         this.#running = this.#run()
//     }

//     static async init(config: BroadcasterConfig): Promise<Broadcast> {
//         const client = new Client({ url: config.url, fingerprint: config.fingerprint, role: "publisher" })
//         const connection = await client.connect();

//         return new Broadcast({ connection, namespace: config.namespace })
//     }

//     async #run() {
//         try {
//             await this.connection.announce(this.namespace)
//             this.subscribed = true
//         } catch (error) {

//             this.subscribed = false
//         }

//         for (; ;) {
//             const subscriber = await this.connection.subscribed()

//             if (!subscriber) {
//                 this.subscribed = false

//                 break
//             }

//             await subscriber.ack()

//             this.subscriber = subscriber

//             this.subscribed = true

//             const bytes = Catalog.encode(this.catalog);

//             const stream = await subscriber.group({ group: 0 });

//             await stream.write({ object: 0, payload: bytes })

//             this.stream = stream
//         }
//     }

//     isSubscribed(): boolean {
//         return this.subscribed;
//     }

//     // async #serveSubscribe(subscriber: SubscribeRecv) {
//     //     try {

//     //         // Send a SUBSCRIBE_OK
//     //         await subscriber.ack()

//     //         console.log("catalog track name:", subscriber.track)

//     //         const stream = await subscriber.group({ group: 0 });

//     //         // const bytes = this.catalog.encode("Hello World")

//     //         await stream.write({ object: 0, payload: bytes })



//     //     } catch (e) {
//     //         const err = asError(e)
//     //         await subscriber.close(1n, `failed to process publish: ${err.message}`)
//     //     } finally {
//     //         // TODO we can't close subscribers because there's no support for clean termination
//     //         // await subscriber.close()
//     //     }
//     // }

//     // async mouseUpdatePosition({ x, y }: { x: number, y: number }, stream: GroupWriter) {

//     //     const mouse_move = {
//     //         input_type: "mouse_move",
//     //         delta_y: y,
//     //         delta_x: x,
//     //     }

//     //     const bytes = Catalog.encode(this.catalog)

//     //     await stream.write({ object: 0, payload: bytes });
//     // }

//     // async mouseUpdateButtons(e: MouseEvent, stream: GroupWriter) {
//     //     const data: { input_type?: "mouse_key_down" | "mouse_key_up"; button: number; } = { button: e.button };

//     //     if (e.type === "mousedown") {
//     //         data["input_type"] = "mouse_key_down"
//     //     } else if (e.type === "mouseup") {
//     //         data["input_type"] = "mouse_key_up"
//     //     }

//     //     const bytes = Catalog.encode(this.catalog)

//     //     await stream.write({ object: 0, payload: bytes });
//     // }

//     // async mouseUpdateWheel(e: WheelEvent, stream: GroupWriter) {
//     //     const data: { input_type?: "mouse_wheel_up" | "mouse_wheel_down" } = {}

//     //     if (e.deltaY < 0.0) {
//     //         data["input_type"] = "mouse_wheel_up"
//     //     } else {
//     //         data["input_type"] = "mouse_wheel_down"
//     //     }

//     //     const bytes = Catalog.encode(this.catalog)

//     //     await stream.write({ object: 0, payload: bytes });
//     // }

//     // async updateKeyUp(e: KeyboardEvent, stream: GroupWriter) {
//     //     const data = {
//     //         input_type: "key_up",
//     //         key_code: e.keyCode
//     //     }

//     //     const bytes = Catalog.encode(this.catalog)

//     //     await stream.write({ object: 0, payload: bytes });
//     // }

//     // async updateKeyDown(e: KeyboardEvent, stream: GroupWriter) {
//     //     const data = {
//     //         input_type: "key_down",
//     //         key_code: e.keyCode
//     //     }

//     //     const bytes = Catalog.encode(this.catalog)

//     //     await stream.write({ object: 0, payload: bytes });
//     // }

//     close() {
//         // TODO implement publish close
//     }

//     // Returns the error message when the connection is closed
//     async closed(): Promise<Error> {
//         try {
//             await this.#running
//             return new Error("closed") // clean termination
//         } catch (e) {
//             return asError(e)
//         }
//     }
// }