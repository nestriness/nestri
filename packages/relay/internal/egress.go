package relay

import (
	"github.com/gorilla/websocket"
	"github.com/pion/webrtc/v4"
	"log"
)

func participantHandler(participant *Participant, room *Room) {
	// Callback for closing PeerConnection
	onPCClose := func() {
		if GetFlags().Verbose {
			log.Printf("Closed PeerConnection for participant: '%s'\n", participant.ID)
		}
		room.removeParticipantByID(participant.ID)
	}

	var err error
	participant.PeerConnection, err = CreatePeerConnection(onPCClose)
	if err != nil {
		log.Printf("Failed to create PeerConnection for participant: '%s' - reason: %s\n", participant.ID, err)
		return
	}

	participant.PeerConnection.OnDataChannel(func(d *webrtc.DataChannel) {
		if GetFlags().Verbose {
			log.Printf("New DataChannel for participant: '%s' - '%s'\n", participant.ID, d.Label())
		}

		// Register channel opening handling
		d.OnOpen(func() {
			if GetFlags().Verbose {
				log.Printf("DataChannel for participant: '%s' - '%s' open\n", participant.ID, d.Label())
			}
		})

		// Register text message handling
		d.OnMessage(func(msg webrtc.DataChannelMessage) {
			room.broadcastMessage(msg, participant.ID) // Exclude the sender
		})

		// Register channel closing handling
		d.OnClose(func() {
			if GetFlags().Verbose {
				log.Printf("DataChannel for participant: '%s' - '%s' closed\n", participant.ID, d.Label())
			}
		})

		participant.DataChannel = d
	})

	participant.PeerConnection.OnICECandidate(func(candidate *webrtc.ICECandidate) {
		if candidate == nil {
			return
		}
		if GetFlags().Verbose {
			log.Printf("ICE candidate for participant: '%s' in room: '%s'\n", participant.ID, room.Name)
		}
		err = participant.WebSocket.SendICECandidateMessage(candidate.ToJSON())
		if err != nil {
			log.Printf("Failed to send ICE candidate for participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
		}
	})

	// Add room tracks for participant
	if room.AudioTrack != nil {
		if err = participant.addTrack(&room.AudioTrack); err != nil {
			log.Printf("Failed to add audio track to participant: '%s' - reason: %s\n", participant.ID, err)
		}
	}
	if room.VideoTrack != nil {
		if err = participant.addTrack(&room.VideoTrack); err != nil {
			log.Printf("Failed to add video track to participant: '%s' - reason: %s\n", participant.ID, err)
		}
	}

	iceHolder := make([]webrtc.ICECandidateInit, 0)

	for {
		// Read message from participant
		msgType, msgData, err := participant.WebSocket.ReadMessage()
		if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure, websocket.CloseNoStatusReceived) {
			// If unexpected close error, break
			if GetFlags().Verbose {
				log.Printf("Unexpected close error from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
			}
			break
		} else if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway, websocket.CloseAbnormalClosure, websocket.CloseNoStatusReceived) {
			// If closing, just break
			if GetFlags().Verbose {
				log.Printf("Closing from participant: '%s' in room: '%s'\n", participant.ID, room.Name)
			}
			break
		} else if err != nil {
			log.Printf("Failed to read message from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
			break
		}

		// Handle message type
		switch msgType {
		case websocket.TextMessage:
			// Ignore, we use binary messages
			if GetFlags().Verbose {
				log.Printf("Received text message from participant: '%s'\n", string(msgData))
			}
			continue
		case websocket.BinaryMessage:
			// Decode message
			var msg WSMessageBase
			if err = DecodeMessage(msgData, &msg); err != nil {
				log.Printf("Failed to decode message from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
				continue
			}

			if GetFlags().Verbose {
				log.Printf("Received message from participant: '%s' in room: '%s' - %s\n", participant.ID, room.Name, msg.PayloadType)
			}

			// Handle message type
			switch msg.PayloadType {
			case "log":
				var logMsg WSMessageLog
				if err = DecodeMessage(msgData, &logMsg); err != nil {
					log.Printf("Failed to decode log message from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
					continue
				}
				// TODO: Handle log message sending to metrics server
			case "metrics":
				// Ignore for now
				continue
			case "sdp":
				var sdpMsg WSMessageSDP
				if err = DecodeMessage(msgData, &sdpMsg); err != nil {
					log.Printf("Failed to decode SDP message from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
					continue
				}
				// Handle SDP message
				answer := handleParticipantSDP(participant, sdpMsg)
				if answer != nil {
					if err = participant.WebSocket.SendSDPMessage(*answer); err != nil {
						log.Printf("Failed to send SDP answer to participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
					}
				} else {
					log.Printf("Failed to handle SDP message from participant: '%s' in room: '%s'\n", participant.ID, room.Name)
				}
			case "ice":
				var iceMsg WSMessageICECandidate
				if err = DecodeMessage(msgData, &iceMsg); err != nil {
					log.Printf("Failed to decode ICE message from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
					continue
				}
				candidate := webrtc.ICECandidateInit{
					Candidate: iceMsg.Candidate.Candidate,
				}
				if participant.PeerConnection.RemoteDescription() != nil {
					if err = participant.PeerConnection.AddICECandidate(candidate); err != nil {
						log.Printf("Failed to add ICE candidate from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
					}
					// Add held ICE candidates
					for _, heldCandidate := range iceHolder {
						if err = participant.PeerConnection.AddICECandidate(heldCandidate); err != nil {
							log.Printf("Failed to add held ICE candidate from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
						}
					}
					iceHolder = nil
				} else {
					iceHolder = append(iceHolder, candidate)
				}
			default:
				log.Printf("Unknown message type from participant: '%s' in room: '%s'\n", participant.ID, room.Name)
			}
		default:
			log.Println("Unknown message type from participant: ", msgType)
		}
	}
}

// SDP offer handler for participants, returns SDP answer
func handleParticipantSDP(participant *Participant, offerMsg WSMessageSDP) *webrtc.SessionDescription {
	var err error

	// Get SDP offer
	sdpOffer := offerMsg.SDP.SDP

	// Set new remote description
	err = participant.PeerConnection.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  sdpOffer,
	})
	if err != nil {
		log.Printf("Failed to set remote description for participant: '%s' - reason: %s\n", participant.ID, err)
		return nil
	}

	// Create SDP answer
	answer, err := participant.PeerConnection.CreateAnswer(nil)
	if err != nil {
		log.Printf("Failed to create SDP answer for participant: '%s' - reason: %s\n", participant.ID, err)
		return nil
	}

	// Set local description
	err = participant.PeerConnection.SetLocalDescription(answer)
	if err != nil {
		log.Printf("Failed to set local description for participant: '%s' - reason: %s\n", participant.ID, err)
		return nil
	}

	// Return SDP answer
	return &answer
}
