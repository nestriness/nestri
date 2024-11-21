export class WebRTCStream {
    private readonly _url: string;
    private _pc: RTCPeerConnection;
    private _mediaStream: MediaStream | undefined = undefined;
    private _dataChannel: RTCDataChannel | undefined;

    constructor(serverURL: string) {
        this._url = serverURL;

        console.log("Setting up PeerConnection");
        this._pc = new RTCPeerConnection({
            iceServers: [
                {
                    urls: "stun:stun.l.google.com:19302"
                }
            ],
        });

        // Allow us to receive single audio and video tracks
        this._pc.addTransceiver("audio", { direction: "recvonly" });
        this._pc.addTransceiver("video", { direction: "recvonly" });

        this._pc.ontrack = (e) => {
            console.log("Track received: ", e.track);
            // Update our mediastream as we receive tracks
            this._mediaStream = e.streams[e.streams.length - 1];
        };
    }

    // Forces opus to stereo in Chromium browsers, because of course
    private forceOpusStereo(SDP: string): string {
        // Look for "minptime=10;useinbandfec=1" and replace with "minptime=10;useinbandfec=1;stereo=1;sprop-stereo=1;"
        return SDP.replace(/(minptime=10;useinbandfec=1)/, "$1;stereo=1;sprop-stereo=1;");
    }

    public async connect(streamName: string) {
        // Create Data Channel
        this._dataChannel = this._pc.createDataChannel("input");
        this._setupDataChannelEvents();

        const offer = await this._pc.createOffer();
        offer.sdp = this.forceOpusStereo(offer.sdp!);
        await this._pc.setLocalDescription(offer);


        const response = await fetch(`${this._url}/api/whep/${streamName}`, {
            method: "POST",
            body: offer.sdp,
            headers: {
                'Content-Type': "application/sdp"
            }
        });

        const answer = await response.json() as RTCSessionDescriptionInit;
        await this._pc.setRemoteDescription(answer);
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
