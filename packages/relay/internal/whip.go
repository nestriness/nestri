package relay

import (
	"encoding/json"
	"errors"
	"fmt"
	"github.com/pion/webrtc/v4"
	"io"
	"log"
	"net/http"
)

// WHIP - WebRTC HTTP Ingress Protocol
// This is the handler for the /api/whip/{roomName} endpoint
func whipHandler(w http.ResponseWriter, r *http.Request) {
	// Get room name
	roomName := r.PathValue("roomName")
	if len(roomName) <= 0 {
		logHTTPError(w, "no room name given", http.StatusBadRequest)
		return
	}

	// Get body
	body, err := io.ReadAll(r.Body)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Get SDP offer from body
	sdpOffer := string(body)
	if sdpOffer == "" {
		// If room exists, just return OK (force room close?)
		if _, ok := Rooms[roomName]; ok {
			w.WriteHeader(http.StatusOK)
			return
		} else {
			logHTTPError(w, "SDP offer not set", http.StatusBadRequest)
			return
		}
	}

	// Verify there's no existing room with same name
	if _, ok := Rooms[roomName]; ok {
		logHTTPError(w, "room name already in use", http.StatusBadRequest)
		return
	}

	// Callback for closing PeerConnection
	onPCClose := func() {
		if GetFlags().Verbose {
			log.Println("Closed PeerConnection for room: ", roomName)
		}
		delete(Rooms, roomName)
	}

	// Create a new room
	if GetFlags().Verbose {
		log.Println("Creating new room: ", roomName)
	}

	var room *Room
	if _, ok := Rooms[roomName]; !ok {
		room = NewRoom(roomName)
		Rooms[roomName] = room
		if GetFlags().Verbose {
			log.Printf("> Created new room %s\n", roomName)
		}
	} else {
		logHTTPError(w, "Room with that name already exists", http.StatusBadRequest)
		return
	}

	room.PeerConnection, err = CreatePeerConnection(onPCClose)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	room.PeerConnection.OnTrack(func(remoteTrack *webrtc.TrackRemote, receiver *webrtc.RTPReceiver) {
		var localTrack *webrtc.TrackLocalStaticRTP
		if remoteTrack.Kind() == webrtc.RTPCodecTypeVideo {
			if GetFlags().Verbose {
				log.Println("Received video track for room: ", roomName)
			}
			localTrack, err = webrtc.NewTrackLocalStaticRTP(remoteTrack.Codec().RTPCodecCapability, "video", fmt.Sprint("nestri-", roomName))
			if err != nil {
				log.Println("Failed to create local video track for room: ", roomName, " - reason: ", err)
				return
			}
			room.VideoTrack = localTrack
		} else if remoteTrack.Kind() == webrtc.RTPCodecTypeAudio {
			if GetFlags().Verbose {
				log.Println("Received audio track for room: ", roomName)
			}
			localTrack, err = webrtc.NewTrackLocalStaticRTP(remoteTrack.Codec().RTPCodecCapability, "audio", fmt.Sprint("nestri-", roomName))
			if err != nil {
				log.Println("Failed to create local audio track for room: ", roomName, " - reason: ", err)
				return
			}
			room.AudioTrack = localTrack
		}

		// TODO: With custom (non-WHEP) viewer connections, notify them of new room to set their tracks

		rtpBuffer := make([]byte, 1400)
		for {
			read, _, err := remoteTrack.Read(rtpBuffer)
			if err != nil {
				// EOF is expected when stopping room
				if !errors.Is(err, io.EOF) {
					log.Println("RTP read error from room: ", roomName, " - ", err)
				}
				break
			}

			_, err = localTrack.Write(rtpBuffer[:read])
			if err != nil && !errors.Is(err, io.ErrClosedPipe) {
				log.Println("RTP write error from room: ", roomName, " - ", err)
				break
			}
		}
		// TODO: Cleanup track from participant for room restart
	})

	room.PeerConnection.OnDataChannel(func(d *webrtc.DataChannel) {
		if GetFlags().Verbose {
			log.Printf("New Room DataChannel '%s'-'%d'\n", d.Label(), d.ID())
		}

		// Register channel opening handling
		d.OnOpen(func() {
			if GetFlags().Verbose {
				log.Printf("Room Data channel '%s'-'%d' open\n", d.Label(), d.ID())
			}
		})

		// Register text message handling
		d.OnMessage(func(msg webrtc.DataChannelMessage) {
			// Log
			log.Printf("Room Data channel message: %v", msg)
		})

		// Register channel closing handling
		d.OnClose(func() {
			if GetFlags().Verbose {
				log.Printf("Room Data channel '%s'-'%d' closed\n", d.Label(), d.ID())
			}
		})

		room.DataChannel = d
	})

	// Set new remote description
	err = room.PeerConnection.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  sdpOffer,
	})
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	gatherComplete := webrtc.GatheringCompletePromise(room.PeerConnection)

	// Create Answer and set local description
	answer, err := room.PeerConnection.CreateAnswer(nil)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	err = room.PeerConnection.SetLocalDescription(answer)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	<-gatherComplete

	// Create a struct to hold the description in a JSON-friendly format
	descJSON := struct {
		Type string `json:"type"`
		Sdp  string `json:"sdp"`
	}{
		Type: answer.Type.String(),
		Sdp:  answer.SDP,
	}

	jsonDesc, err := json.Marshal(descJSON)
	if err != nil {
		log.Println(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Return SDP answer
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Location", fmt.Sprint("/api/whip/", roomName))
	w.WriteHeader(http.StatusCreated)
	_, err = w.Write(jsonDesc)
	if err != nil {
		log.Println(err)
		return
	}
}
