package relay

import (
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

	// Data channel settings
	settingOrdered := false
	settingMaxRetransmits := uint16(0)
	participant.DataChannel, err = participant.PeerConnection.CreateDataChannel("data", &webrtc.DataChannelInit{
		Ordered:        &settingOrdered,
		MaxRetransmits: &settingMaxRetransmits,
	})
	if err != nil {
		log.Printf("Failed to create data channel for participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
		return
	}

	// Register channel opening handling
	participant.DataChannel.OnOpen(func() {
		if GetFlags().Verbose {
			log.Printf("DataChannel open for participant: %s\n", participant.ID)
		}
	})

	// Register text message handling
	participant.DataChannel.OnMessage(func(msg webrtc.DataChannelMessage) {
		room.broadcastMessage(msg, participant.ID) // Exclude the sender
	})

	// Register channel closing handling
	participant.DataChannel.OnClose(func() {
		if GetFlags().Verbose {
			log.Printf("DataChannel closed for participant: %s\n", participant.ID)
		}
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

	iceHolder := make([]webrtc.ICECandidateInit, 0)

	// ICE callback
	participant.WebSocket.RegisterMessageCallback("ice", func(data []byte) {
		var iceMsg WSMessageICECandidate
		if err = DecodeMessage(data, &iceMsg); err != nil {
			log.Printf("Failed to decode ICE message from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
			return
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
	})

	// SDP answer callback
	participant.WebSocket.RegisterMessageCallback("sdp", func(data []byte) {
		var sdpMsg WSMessageSDP
		if err = DecodeMessage(data, &sdpMsg); err != nil {
			log.Printf("Failed to decode SDP message from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
			return
		}
		handleParticipantSDP(participant, sdpMsg)
	})

	// Log callback
	participant.WebSocket.RegisterMessageCallback("log", func(data []byte) {
		var logMsg WSMessageLog
		if err = DecodeMessage(data, &logMsg); err != nil {
			log.Printf("Failed to decode log message from participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
			return
		}
		// TODO: Handle log message sending to metrics server
	})

	// Metrics callback
	participant.WebSocket.RegisterMessageCallback("metrics", func(data []byte) {
		// Ignore for now
	})

	participant.WebSocket.RegisterOnClose(func() {
		if GetFlags().Verbose {
			log.Printf("WebSocket closed for participant: '%s' in room: '%s'\n", participant.ID, room.Name)
		}
		// Remove from Room
		room.removeParticipantByID(participant.ID)
	})

	log.Printf("Participant: '%s' in room: '%s' is now ready, sending an OK\n", participant.ID, room.Name)
	if err = participant.WebSocket.SendAnswerMessage(AnswerOK); err != nil {
		log.Printf("Failed to send OK answer for participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
	}

	// If room is already online, send also offer
	if room.Online {
		if room.AudioTrack != nil {
			if err = participant.addTrack(&room.AudioTrack); err != nil {
				log.Printf("Failed to add audio track for participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
			}
		}
		if room.VideoTrack != nil {
			if err = participant.addTrack(&room.VideoTrack); err != nil {
				log.Printf("Failed to add video track for participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
			}
		}
		if err = participant.signalOffer(); err != nil {
			log.Printf("Failed to signal offer for participant: '%s' in room: '%s' - reason: %s\n", participant.ID, room.Name, err)
		}
	}
}

// SDP answer handler for participants
func handleParticipantSDP(participant *Participant, answerMsg WSMessageSDP) {
	// Get SDP offer
	sdpAnswer := answerMsg.SDP.SDP

	// Set remote description
	err := participant.PeerConnection.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.SDPTypeAnswer,
		SDP:  sdpAnswer,
	})
	if err != nil {
		log.Printf("Failed to set remote description for participant: '%s' - reason: %s\n", participant.ID, err)
	}
}
