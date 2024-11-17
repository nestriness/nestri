export class WebRTCStream {
    private readonly _url: string;
    private _pc: RTCPeerConnection;
    private _mediaStream: MediaStream | undefined = undefined;

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

        const answer = await response.text();
        await this._pc.setRemoteDescription({
            sdp: answer,
            type: "answer",
        });
    }

    public getMediaStream() {
        return this._mediaStream;
    }
}
