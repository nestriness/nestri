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
		logHTTPError(w, "no room name given", http.StatusBadRequest)
		return
	}

	// Make sure room exists
	room, ok := Rooms[roomName]
	if !ok {
		logHTTPError(w, "no room with given name online", http.StatusNotFound)
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
		if GetFlags().Verbose {
			log.Println("Closed PeerConnection for participant: ", participantName, " - belonging to room: ", roomName)
		}
		room.removeParticipantByName(participantName)
	}

	// Create new participant
	if GetFlags().Verbose {
		log.Println("New participant for room: ", roomName)
	}
	participant := NewParticipant(participantName)

	participant.PeerConnection, err = CreatePeerConnection(onPCClose)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	participant.PeerConnection.OnDataChannel(func(d *webrtc.DataChannel) {
		if GetFlags().Verbose {
			log.Printf("New DataChannel '%s'-'%d'\n", d.Label(), d.ID())
		}

		// Register channel opening handling
		d.OnOpen(func() {
			if GetFlags().Verbose {
				log.Printf("Data channel '%s'-'%d' open\n", d.Label(), d.ID())
			}
			room.addParticipant(participant)
		})

		// Register text message handling
		d.OnMessage(func(msg webrtc.DataChannelMessage) {
			room.broadcastMessage(msg, participant.ID) // Exclude the sender
		})

		// Register channel closing handling
		d.OnClose(func() {
			if GetFlags().Verbose {
				log.Printf("Data channel '%s'-'%d' closed\n", d.Label(), d.ID())
			}
			room.removeParticipantByID(participant.ID)
		})
	})

	// Add room tracks for participant
	if room.AudioTrack != nil {
		if err = participant.addTrack(&room.AudioTrack); err != nil {
			logHTTPError(w, "failed to add audio track to participant", http.StatusInternalServerError)
			return
		}
	} else if GetFlags().Verbose {
		log.Println("nil audio track for room: ", roomName)
	}
	if room.VideoTrack != nil {
		if err = participant.addTrack(&room.VideoTrack); err != nil {
			logHTTPError(w, "failed to add video track to participant", http.StatusInternalServerError)
			return
		}
	} else if GetFlags().Verbose {
		log.Println("nil video track for room: ", roomName)
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
