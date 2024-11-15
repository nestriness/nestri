import type * as Party from "partykit/server";

type Message = {
  type: "input" | "text"
  message: any;
}

export default class Server implements Party.Server {
  connections: string[]; //saves all the participants' ids

  constructor(readonly room: Party.Room) {
    this.connections = []
  }

  onConnect(conn: Party.Connection, ctx: Party.ConnectionContext) {
    // A websocket just connected!
    console.log(
      `Connected:
  id: ${conn.id}
  room: ${this.room.id}
  url: ${new URL(ctx.request.url).pathname}`
    );

    this.connections.push(conn.id)
  }

  onMessage(message: string, sender: Party.Connection) {
    // let's log the message
    console.log(`connection ${sender.id} sent message: ${message}`);
    //TODO: If it is of type input... broadcast to server only!
    // as well as broadcast it to all the other connections in the room...

    const msg = JSON.parse(message) as Message;

    switch (msg.type) {
      case "input":
        //TODO: Check whether the server is connected
        // send input information only to the `nestri-server` running the game
        this.room.broadcast(
          JSON.stringify(msg.message),
          this.connections.filter((v) => v !== "nestri-server")
        )
        break;
      default:
        break;
    }

    // this.room.broadcast(
    //   message,
    //   // ...except for the connection it came from
    //   [sender.id]
    // );
  }
}

Server satisfies Party.Worker;
