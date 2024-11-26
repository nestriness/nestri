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
  private _ws: WebSocket;
  private _pc: RTCPeerConnection;
  private _mediaStream: MediaStream | undefined = undefined;
  private _dataChannel: RTCDataChannel | undefined;
  private _onConnected: () => void;

  constructor(serverURL: string, roomName: string, connectedCallback: () => void) {
    // If roomName is not provided, return
    if (roomName.length <= 0) {
      console.error("Room name not provided");
      return;
    }

    this._onConnected = connectedCallback;

    console.log("Setting up WebSocket");
    // Replace http/https with ws/wss
    const wsURL = serverURL.replace(/^http/, "ws");
    this._ws = new WebSocket(`${wsURL}/api/ws-participant/${roomName}`);
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

      // Allow us to receive single audio and video tracks
      this._pc.addTransceiver("audio", {direction: "recvonly"});
      this._pc.addTransceiver("video", {direction: "recvonly"});

      this._pc.ontrack = (e) => {
        console.log("Track received: ", e.track);
        // Update our mediastream as we receive tracks
        this._mediaStream = e.streams[e.streams.length - 1];
        // If both audio and video tracks are received, call the connected callback
        if (this._mediaStream.getAudioTracks().length > 0 && this._mediaStream.getVideoTracks().length > 0 && this._onConnected) {
          this._onConnected();
          // Prevent further calls to the connected callback
          this._onConnected = () => {};
        }
      };

      this._pc.onicecandidate = (e) => {
        if (e.candidate) {
          const message: WSMessageICE = {
            payload_type: "ice",
            candidate: e.candidate
          };
          this._ws.send(encodeMessage(message));
        }
      }

      // Create Data Channel
      this._dataChannel = this._pc.createDataChannel("input");
      this._setupDataChannelEvents();

      const offer = await this._pc.createOffer();
      offer.sdp = this.forceOpusStereo(offer.sdp!);
      await this._pc.setLocalDescription(offer);

      const message: WSMessageSDP = {
        payload_type: "sdp",
        sdp: offer
      };
      this._ws.send(encodeMessage(message));
    }

    let iceHolder: RTCIceCandidateInit[] = [];

    this._ws.onmessage = async (e) => {
      // allow only binary
      if (typeof e.data !== "object") return;
      if (!e.data) return;
      const message = await decodeMessage<WSMessageBase>(e.data);
      switch (message.payload_type) {
        case "sdp":
          this._pc.setRemoteDescription((message as WSMessageSDP).sdp);
          break;
        case "ice":
          // If remote description is not set yet, hold the ICE candidates
          if (this._pc.remoteDescription) {
            this._pc.addIceCandidate((message as WSMessageICE).candidate);
            // Add held ICE candidates
            iceHolder.forEach(candidate => this._pc.addIceCandidate(candidate));
            iceHolder = [];
          } else {
            iceHolder.push((message as WSMessageICE).candidate);
          }
          break;
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

  public getMediaStream() {
    return this._mediaStream;
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
