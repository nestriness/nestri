package relay

import (
	"errors"
	"fmt"
	"github.com/gorilla/websocket"
	"github.com/pion/webrtc/v4"
	"io"
	"log"
	"strings"
)

func ingestHandler(room *Room) {
	// Callback for closing PeerConnection
	onPCClose := func() {
		if GetFlags().Verbose {
			log.Printf("Closed PeerConnection for room: '%s'\n", room.Name)
		}
		room.Online = false
		DeleteRoomIfEmpty(room)
	}

	var err error
	room.PeerConnection, err = CreatePeerConnection(onPCClose)
	if err != nil {
		log.Printf("Failed to create PeerConnection for room: '%s' - reason: %s\n", room.Name, err)
		return
	}

	room.PeerConnection.OnTrack(func(remoteTrack *webrtc.TrackRemote, receiver *webrtc.RTPReceiver) {
		var localTrack *webrtc.TrackLocalStaticRTP
		if remoteTrack.Kind() == webrtc.RTPCodecTypeVideo {
			if GetFlags().Verbose {
				log.Printf("Received video track for room: '%s'\n", room.Name)
			}
			localTrack, err = webrtc.NewTrackLocalStaticRTP(remoteTrack.Codec().RTPCodecCapability, "video", fmt.Sprint("nestri-", room.Name))
			if err != nil {
				log.Printf("Failed to create local video track for room: '%s' - reason: %s\n", room.Name, err)
				return
			}
			room.VideoTrack = localTrack
		} else if remoteTrack.Kind() == webrtc.RTPCodecTypeAudio {
			if GetFlags().Verbose {
				log.Printf("Received audio track for room: '%s'\n", room.Name)
			}
			localTrack, err = webrtc.NewTrackLocalStaticRTP(remoteTrack.Codec().RTPCodecCapability, "audio", fmt.Sprint("nestri-", room.Name))
			if err != nil {
				log.Printf("Failed to create local audio track for room: '%s' - reason: %s\n", room.Name, err)
				return
			}
			room.AudioTrack = localTrack
		}

		// If both audio and video tracks are set, set online state
		if room.AudioTrack != nil && room.VideoTrack != nil {
			room.Online = true
			if GetFlags().Verbose {
				log.Printf("Room online and receiving: '%s'\n", room.Name)
			}
		}

		rtpBuffer := make([]byte, 1400)
		for {
			read, _, err := remoteTrack.Read(rtpBuffer)
			if err != nil {
				// EOF is expected when stopping room
				if !errors.Is(err, io.EOF) {
					log.Printf("RTP read error from room: '%s' - reason: %s\n", room.Name, err)
				}
				break
			}

			_, err = localTrack.Write(rtpBuffer[:read])
			if err != nil && !errors.Is(err, io.ErrClosedPipe) {
				log.Printf("Failed to write RTP to local track for room: '%s' - reason: %s\n", room.Name, err)
				break
			}
		}

		if remoteTrack.Kind() == webrtc.RTPCodecTypeVideo {
			room.VideoTrack = nil
		} else if remoteTrack.Kind() == webrtc.RTPCodecTypeAudio {
			room.AudioTrack = nil
		}

		if room.VideoTrack == nil && room.AudioTrack == nil {
			room.Online = false
			if GetFlags().Verbose {
				log.Printf("Room offline and not receiving: '%s'\n", room.Name)
			}
			DeleteRoomIfEmpty(room)
		}
	})

	room.PeerConnection.OnDataChannel(func(d *webrtc.DataChannel) {
		if GetFlags().Verbose {
			log.Printf("New DataChannel for room: '%s' - '%s'\n", room.Name, d.Label())
		}

		// Register channel opening handling
		d.OnOpen(func() {
			if GetFlags().Verbose {
				log.Printf("DataChannel for room: '%s' - '%s' open\n", room.Name, d.Label())
			}
		})

		// Register text message handling
		d.OnMessage(func(msg webrtc.DataChannelMessage) {
			if GetFlags().Verbose {
				log.Printf("DataChannel for room: '%s' - '%s' message: %s\n", room.Name, d.Label(), msg.Data)
			}
		})

		// Register channel closing handling
		d.OnClose(func() {
			if GetFlags().Verbose {
				log.Printf("DataChannel for room: '%s' - '%s' closed\n", room.Name, d.Label())
			}
		})

		room.DataChannel = d
	})

	room.PeerConnection.OnICECandidate(func(candidate *webrtc.ICECandidate) {
		if candidate == nil {
			return
		}
		if GetFlags().Verbose {
			log.Printf("ICE candidate for room: '%s'\n", room.Name)
		}
		err = room.WebSocket.SendICECandidateMessage(candidate.ToJSON())
		if err != nil {
			log.Printf("Failed to send ICE candidate for room: '%s' - reason: %s\n", room.Name, err)
		}
	})

	iceHolder := make([]webrtc.ICECandidateInit, 0)

	// Handle WebSocket messages from ingest
	for {
		// Read message from ingest
		msgType, msgData, err := room.WebSocket.ReadMessage()
		if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure, websocket.CloseNoStatusReceived) {
			// If unexpected close error, break
			if GetFlags().Verbose {
				log.Printf("Unexpected close error from ingest for room: '%s' - reason: %s\n", room.Name, err)
			}
			break
		} else if websocket.IsCloseError(err, websocket.CloseNormalClosure, websocket.CloseGoingAway, websocket.CloseAbnormalClosure, websocket.CloseNoStatusReceived) {
			// If closing, just break
			if GetFlags().Verbose {
				log.Printf("Closing from ingest for room: '%s'\n", room.Name)
			}
			break
		} else if err != nil {
			log.Printf("Failed to read message from ingest for room: '%s' - reason: %s\n", room.Name, err)
			break
		}

		// Handle message type
		switch msgType {
		case websocket.TextMessage:
			// Ignore, we use binary messages
			if GetFlags().Verbose {
				log.Printf("Received text message from ingest: '%s'\n", string(msgData))
			}
			continue
		case websocket.BinaryMessage:
			// Decode message
			var msg WSMessageBase
			if err = DecodeMessage(msgData, &msg); err != nil {
				log.Printf("Failed to decode message from ingest for room: '%s' - reason: %s\n", room.Name, err)
				continue
			}

			if GetFlags().Verbose {
				log.Printf("Received message from ingest for room: '%s' - %s\n", room.Name, msg.PayloadType)
			}

			// Handle message type
			switch msg.PayloadType {
			case "log":
				var logMsg WSMessageLog
				if err = DecodeMessage(msgData, &logMsg); err != nil {
					log.Printf("Failed to decode log message from ingest for room: '%s' - reason: %s\n", room.Name, err)
					continue
				}
				// TODO: Handle log message sending to metrics server
			case "metrics":
				var metricsMsg WSMessageMetrics
				if err = DecodeMessage(msgData, &metricsMsg); err != nil {
					log.Printf("Failed to decode metrics message from ingest for room: '%s' - reason: %s\n", room.Name, err)
					continue
				}
				if GetFlags().Verbose {
					log.Printf("Metrics message from ingest for room: '%s'\n", room.Name)
					log.Printf("  CPU: %f\n", metricsMsg.UsageCPU)
					log.Printf("  Memory: %f\n", metricsMsg.UsageMemory)
					log.Printf("  Uptime: %d\n", metricsMsg.Uptime)
					log.Printf("  Pipeline latency: %f\n", metricsMsg.PipelineLatency)
				}
			case "sdp":
				var sdpMsg WSMessageSDP
				if err = DecodeMessage(msgData, &sdpMsg); err != nil {
					log.Printf("Failed to decode SDP message from ingest for room: '%s' - reason: %s\n", room.Name, err)
					continue
				}
				answer := handleIngestSDP(room, sdpMsg)
				if answer != nil {
					if err = room.WebSocket.SendSDPMessage(*answer); err != nil {
						log.Printf("Failed to send SDP answer to ingest for room: '%s' - reason: %s\n", room.Name, err)
					}
				} else {
					log.Printf("Failed to handle SDP message from ingest for room: '%s'\n", room.Name)
				}
			case "ice":
				var iceMsg WSMessageICECandidate
				if err = DecodeMessage(msgData, &iceMsg); err != nil {
					log.Printf("Failed to decode ICE candidate message from ingest for room: '%s' - reason: %s\n", room.Name, err)
					continue
				}
				candidate := webrtc.ICECandidateInit{
					Candidate: iceMsg.Candidate.Candidate,
				}
				if room.PeerConnection != nil {
					// If remote isn't set yet, store ICE candidates
					if room.PeerConnection.RemoteDescription() != nil {
						if err = room.PeerConnection.AddICECandidate(candidate); err != nil {
							log.Printf("Failed to add ICE candidate for room: '%s' - reason: %s\n", room.Name, err)
						}
						// Add any held ICE candidates
						for _, heldCandidate := range iceHolder {
							if err = room.PeerConnection.AddICECandidate(heldCandidate); err != nil {
								log.Printf("Failed to add held ICE candidate for room: '%s' - reason: %s\n", room.Name, err)
							}
						}
						iceHolder = nil
					} else {
						iceHolder = append(iceHolder, candidate)
					}
				} else {
					log.Printf("ICE candidate received before PeerConnection for room: '%s'\n", room.Name)
				}
			default:
				log.Println("Unknown message type from ingest: ", msg.PayloadType)
			}
		default:
			log.Println("Unknown message type from ingest: ", msgType)
		}
	}

	// Close WebSocket connection
	if err := room.WebSocket.Close(); err != nil {
		log.Printf("Failed to close WebSocket for room: '%s' - reason: %s\n", room.Name, err)
	}
	room.WebSocket = nil

	// If PeerConnection is not open, delete room
	if room.PeerConnection != nil && room.PeerConnection.ConnectionState() != webrtc.PeerConnectionStateConnected {
		DeleteRoomIfEmpty(room)
	}
}

// SDP offer handler, returns SDP answer
func handleIngestSDP(room *Room, offerMsg WSMessageSDP) *webrtc.SessionDescription {
	var err error

	// Get SDP offer
	sdpOffer := offerMsg.SDP.SDP

	// Modify SDP offer to remove opus "sprop-maxcapturerate=24000" (fixes opus bad quality issue, present in GStreamer)
	sdpOffer = strings.Replace(sdpOffer, ";sprop-maxcapturerate=24000", "", -1)

	// Set new remote description
	err = room.PeerConnection.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  sdpOffer,
	})
	if err != nil {
		log.Printf("Failed to set remote description for room: '%s' - reason: %s\n", room.Name, err)
		return nil
	}

	// Create SDP answer
	answer, err := room.PeerConnection.CreateAnswer(nil)
	if err != nil {
		log.Printf("Failed to create SDP answer for room: '%s' - reason: %s\n", room.Name, err)
		return nil
	}

	// Set local description
	err = room.PeerConnection.SetLocalDescription(answer)
	if err != nil {
		log.Printf("Failed to set local description for room: '%s' - reason: %s\n", room.Name, err)
		return nil
	}

	return &answer
}
