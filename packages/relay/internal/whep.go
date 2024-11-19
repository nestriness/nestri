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

	// Make sure room exists
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

	participantName := r.URL.Query().Get("name")
	if participantName == "" {
		participantName = uuid.New().String()
	}

	// Callback for closing PeerConnection
	onPCClose := func() {
		if GetRelayFlags().Verbose {
			log.Println("Closed PeerConnection for viewer: ", participantName, " - belonging to stream: ", roomName)
		}
		if _, ok := Participants[roomName]; ok {
			if _, vOk := Participants[participantName]; vOk {
				delete(Participants[roomName], participantName)
			}
		}
	}

	// Create new participant
	if GetRelayFlags().Verbose {
		log.Println("New viewer for stream: ", roomName)
	}
	participant := &Participant{
		name: participantName,
	}

	participant.PeerConnection, err = CreatePeerConnection(onPCClose)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Add stream tracks for viewer
	if room.AudioTrack != nil {
		if err = participant.AddTrack(&room.AudioTrack); err != nil {
			logHTTPError(w, "failed to add audio track to viewer", http.StatusInternalServerError)
			return
		}
	} else if GetRelayFlags().Verbose {
		log.Println("nil audio track for stream: ", roomName)
	}
	if room.VideoTrack != nil {
		if err = participant.AddTrack(&room.VideoTrack); err != nil {
			logHTTPError(w, "failed to add video track to viewer", http.StatusInternalServerError)
			return
		}
	} else if GetRelayFlags().Verbose {
		log.Println("nil video track for stream: ", roomName)
	}

	// Set new remote description
	err = participant.PeerConnection.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  sdpOffer,
	})
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	gatherComplete := webrtc.GatheringCompletePromise(participant.PeerConnection)

	// Create Answer and set local description
	answer, err := participant.PeerConnection.CreateAnswer(nil)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	err = participant.PeerConnection.SetLocalDescription(answer)
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
	_, err = w.Write([]byte(participant.PeerConnection.LocalDescription().SDP))
	if err != nil {
		log.Println(err)
		return
	}
}
