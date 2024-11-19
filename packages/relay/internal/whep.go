package relay

import (
	"fmt"
	"io"
	"log"
	"net/http"

	"github.com/google/uuid"
	"github.com/pion/webrtc/v4"
)

// WHEP - WebRTC HTTP Egress Protocol
// This is the handler for the /api/whep/{roomName} endpoint
func whepHandler(w http.ResponseWriter, r *http.Request) {
	// Get room name
	roomName := r.PathValue("roomName")
	if len(roomName) <= 0 {
		logHTTPError(w, "no stream name given", http.StatusBadRequest)
		return
	}

	// Make sure stream exists
	room, ok := Rooms[roomName]
	if !ok {
		logHTTPError(w, "no stream with given name online", http.StatusNotFound)
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
		logHTTPError(w, "SDP offer not set", http.StatusBadRequest)
		return
	}

	// Generate UUID for viewer
	viewerName := uuid.New().String()

	// Callback for closing PeerConnection
	onPCClose := func() {
		if GetRelayFlags().Verbose {
			log.Println("Closed PeerConnection for viewer: ", viewerName, " - beloging to stream: ", roomName)
		}
		if _, ok := Participants[roomName]; ok {
			if _, vOk := Participants[viewerName]; vOk {
				delete(Participants[roomName], viewerName)
			}
		}
	}

	// Create new viewer
	if GetRelayFlags().Verbose {
		log.Println("New viewer for stream: ", roomName)
	}
	viewer := &Participant{
		name: viewerName,
	}
	viewer.PeerConnection, err = CreatePeerConnection(onPCClose)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Add stream tracks for viewer
	if room.AudioTrack != nil {
		if err = viewer.AddTrack(&room.AudioTrack); err != nil {
			logHTTPError(w, "failed to add audio track to viewer", http.StatusInternalServerError)
			return
		}
	} else if GetRelayFlags().Verbose {
		log.Println("nil audio track for stream: ", roomName)
	}
	if room.VideoTrack != nil {
		if err = viewer.AddTrack(&room.VideoTrack); err != nil {
			logHTTPError(w, "failed to add video track to viewer", http.StatusInternalServerError)
			return
		}
	} else if GetRelayFlags().Verbose {
		log.Println("nil video track for stream: ", roomName)
	}

	// Set new remote description
	err = viewer.PeerConnection.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  sdpOffer,
	})
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	gatherComplete := webrtc.GatheringCompletePromise(viewer.PeerConnection)

	// Create Answer and set local description
	answer, err := viewer.PeerConnection.CreateAnswer(nil)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	err = viewer.PeerConnection.SetLocalDescription(answer)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	<-gatherComplete

	// Return SDP answer
	w.Header().Set("Content-Type", "application/sdp")
	w.Header().Set("Location", fmt.Sprint("/api/whep/", roomName))
	w.WriteHeader(http.StatusCreated)
	_, err = w.Write([]byte(viewer.PeerConnection.LocalDescription().SDP))
	if err != nil {
		log.Println(err)
		return
	}
}
