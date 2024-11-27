import {gzip, ungzip} from "pako";

interface WSMessageBase {
  payload_type: string;
}

interface WSMessageICE extends WSMessageBase {
  payload_type: "ice";
  candidate: RTCIceCandidateInit;
}

interface WSMessageSDP extends WSMessageBase {
  payload_type: "sdp";
  sdp: RTCSessionDescriptionInit;
}

enum JoinerType {
  JoinerNode = 0,
  JoinerClient = 1,
}

interface WSMessageJoin extends WSMessageBase {
  payload_type: "join";
  joiner_type: JoinerType;
}

enum AnswerType {
  AnswerOffline = 0,
  AnswerInUse,
  AnswerOK
}

interface WSMessageAnswer extends WSMessageBase {
  payload_type: "answer";
  answer_type: AnswerType;
}

function blobToUint8Array(blob: Blob): Promise<Uint8Array> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onloadend = () => {
      const arrayBuffer = reader.result as ArrayBuffer;
      resolve(new Uint8Array(arrayBuffer));
    };
    reader.onerror = reject;
    reader.readAsArrayBuffer(blob);
  });
}

export function encodeMessage<T>(message: T): Uint8Array {
  // Convert the message to JSON string
  const json = JSON.stringify(message);
  // Compress the JSON string using gzip
  return gzip(json);
}

export async function decodeMessage<T>(data: Blob): Promise<T> {
  // Convert the Blob to Uint8Array
  const array = await blobToUint8Array(data);
  // Decompress the gzip data
  const decompressed = ungzip(array);
  // Convert the Uint8Array to JSON string
  const json = new TextDecoder().decode(decompressed);
  // Parse the JSON string
  return JSON.parse(json);
}

export class WebRTCStream {
  private _ws: WebSocket | undefined = undefined;
  private _pc: RTCPeerConnection | undefined = undefined;
  private _mediaStream: MediaStream | undefined = undefined;
  private _dataChannel: RTCDataChannel | undefined = undefined;
  private _onConnected: ((stream: MediaStream | null) => void) | undefined = undefined;

  constructor(serverURL: string, roomName: string, connectedCallback: (stream: MediaStream | null) => void) {
    // If roomName is not provided, return
    if (roomName.length <= 0) {
      console.error("Room name not provided");
      return;
    }

    this._onConnected = connectedCallback;

    console.log("Setting up WebSocket");
    // Replace http/https with ws/wss
    const wsURL = serverURL.replace(/^http/, "ws");
    this._ws = new WebSocket(`${wsURL}/api/ws/${roomName}`);
    this._ws.onopen = async () => {
      console.log("WebSocket opened");

      console.log("Setting up PeerConnection");
      this._pc = new RTCPeerConnection({
        iceServers: [
          {
            urls: "stun:stun.l.google.com:19302"
          }
        ],
      });

      this._pc.ontrack = (e) => {
        console.log("Track received: ", e.track);
        this._mediaStream = e.streams[e.streams.length - 1];
      };

      this._pc.onconnectionstatechange = () => {
        console.log("Connection state: ", this._pc!.connectionState);
        if (this._pc!.connectionState === "connected") {
          if (this._onConnected && this._mediaStream)
            this._onConnected(this._mediaStream);
        }
      };

      this._pc.onicecandidate = (e) => {
        if (e.candidate) {
          const message: WSMessageICE = {
            payload_type: "ice",
            candidate: e.candidate
          };
          this._ws!.send(encodeMessage(message));
        }
      }

      this._pc.ondatachannel = (e) => {
        this._dataChannel = e.channel;
        this._setupDataChannelEvents();
      }

      // Send join message
      const joinMessage: WSMessageJoin = {
        payload_type: "join",
        joiner_type: JoinerType.JoinerClient
      };
      this._ws!.send(encodeMessage(joinMessage));
    }

    let iceHolder: RTCIceCandidateInit[] = [];

    this._ws.onmessage = async (e) => {
      // allow only binary
      if (typeof e.data !== "object") return;
      if (!e.data) return;
      const message = await decodeMessage<WSMessageBase>(e.data);
      switch (message.payload_type) {
        case "sdp":
          await this._pc!.setRemoteDescription((message as WSMessageSDP).sdp);
          // Create our answer
          const answer = await this._pc!.createAnswer();
          // Force stereo in Chromium browsers
          answer.sdp = this.forceOpusStereo(answer.sdp!);
          await this._pc!.setLocalDescription(answer);
          this._ws!.send(encodeMessage({
            payload_type: "sdp",
            sdp: answer
          }));
          break;
        case "ice":
          // If remote description is not set yet, hold the ICE candidates
          if (this._pc!.remoteDescription) {
            await this._pc!.addIceCandidate((message as WSMessageICE).candidate);
            // Add held ICE candidates
            for (const ice of iceHolder) {
              await this._pc!.addIceCandidate(ice);
            }
            iceHolder = [];
          } else {
            iceHolder.push((message as WSMessageICE).candidate);
          }
          break;
        case "answer":
          switch ((message as WSMessageAnswer).answer_type) {
            case AnswerType.AnswerOffline:
              console.log("Room is offline");
              // Call callback with null stream
              if (this._onConnected)
                this._onConnected(null);

              break;
            case AnswerType.AnswerInUse:
              console.warn("Room is in use, we shouldn't even be getting this message");
              break;
            case AnswerType.AnswerOK:
              console.log("Joining Room was successful");
              break;
          }
          break;
        default:
          console.error("Unknown message type: ", message);
      }
    }

    this._ws.onclose = () => {
      console.log("WebSocket closed");
    }

    this._ws.onerror = (e) => {
      console.error("WebSocket error: ", e);
    }
  }

  // Forces opus to stereo in Chromium browsers, because of course
  private forceOpusStereo(SDP: string): string {
    // Look for "minptime=10;useinbandfec=1" and replace with "minptime=10;useinbandfec=1;stereo=1;sprop-stereo=1;"
    return SDP.replace(/(minptime=10;useinbandfec=1)/, "$1;stereo=1;sprop-stereo=1;");
  }

  private _setupDataChannelEvents() {
    if (!this._dataChannel) return;

    this._dataChannel.onclose = () => console.log('sendChannel has closed')
    this._dataChannel.onopen = () => console.log('sendChannel has opened')
    this._dataChannel.onmessage = e => console.log(`Message from DataChannel '${this._dataChannel?.label}' payload '${e.data}'`)
  }

  // Optional: Method to send arbitrary data over the data channel
  public sendData(message: string) {
    if (this._dataChannel && this._dataChannel.readyState === "open") {
      this._dataChannel.send(message);
    } else {
      console.log("Data channel not open or not established.");
    }
  }
}
