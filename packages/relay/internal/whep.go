package relay

import (
	"encoding/json"
	"fmt"
	"github.com/pion/webrtc/v4"
	"io"
	"log"
	"net/http"
	"strings"
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

	// Check if Room exist, create offline one if none
	room := GetRoomByName(roomName)
	if room == nil {
		room = NewRoom(roomName)
		AddRoom(room)
		if GetFlags().Verbose {
			log.Printf("Created new offline room: %s - %v\n", room.Name, room.ID)
		}
	}

	// Create new participant
	participant := NewParticipant()
	if GetFlags().Verbose {
		log.Println("New participant: ", participant.ID, " - for room: ", roomName)
	}
	room.addParticipant(participant)

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

	// Callback for closing PeerConnection
	onPCClose := func() {
		if GetFlags().Verbose {
			log.Println("Closed PeerConnection for participant: ", participant.ID, " - for room: ", roomName)
		}
		room.removeParticipantByID(participant.ID)
	}

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
			if msg.IsString && strings.Contains(string(msg.Data), "candidate") {
				// Handle potential ICE candidate
				var iceCandidate webrtc.ICECandidateInit
				if err = json.Unmarshal(msg.Data, &iceCandidate); err == nil {
					err := participant.PeerConnection.AddICECandidate(iceCandidate)
					if err != nil {
						log.Println("Failed to add ICE candidate for participant: ", participant.ID)
						return
					}
				}
			} else {
				room.broadcastMessage(msg, participant.ID) // Exclude the sender
			}
		})

		// Register channel closing handling
		d.OnClose(func() {
			if GetFlags().Verbose {
				log.Printf("Data channel '%s'-'%d' closed\n", d.Label(), d.ID())
			}
			room.removeParticipantByID(participant.ID)
		})

		participant.DataChannel = d
	})

	participant.PeerConnection.OnICECandidate(func(candidate *webrtc.ICECandidate) {
		// TODO: Trickle ICE
	})

	// Add room tracks for participant
	if room.AudioTrack != nil {
		if err = participant.addTrack(&room.AudioTrack); err != nil {
			logHTTPError(w, "failed to add audio track to participant", http.StatusInternalServerError)
			return
		}
	}
	if room.VideoTrack != nil {
		if err = participant.addTrack(&room.VideoTrack); err != nil {
			logHTTPError(w, "failed to add video track to participant", http.StatusInternalServerError)
			return
		}
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

	localDesc := participant.PeerConnection.LocalDescription()

	// Create a struct to hold the description in a JSON-friendly format
	descJSON := struct {
		Type string `json:"type"`
		Sdp  string `json:"sdp"`
	}{
		Type: localDesc.Type.String(),
		Sdp:  localDesc.SDP,
	}

	jsonDesc, err := json.Marshal(descJSON)
	if err != nil {
		log.Println(err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Return SDP answer
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Location", fmt.Sprint("/api/whep/", roomName))
	w.WriteHeader(http.StatusCreated)
	_, err = w.Write(jsonDesc)
	if err != nil {
		log.Println(err)
		return
	}
}
