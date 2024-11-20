package relay

import (
	"github.com/google/uuid"
	"github.com/pion/webrtc/v4"
)

type Participant struct {
	ID             uuid.UUID //< Internal IDs are useful to keeping unique internal track and not have conflicts later
	Name           string
	PeerConnection *webrtc.PeerConnection
	DataChannel    *webrtc.DataChannel
}

func NewParticipant(participantName string) *Participant {
	return &Participant{
		ID:   uuid.New(),
		Name: participantName,
	}
}

func (vw *Participant) addTrack(trackLocal *webrtc.TrackLocal) error {
	rtpSender, err := vw.PeerConnection.AddTrack(*trackLocal)
	if err != nil {
		return err
	}

	go func() {
		rtcpBuffer := make([]byte, 1400)
		for {
			if _, _, rtcpErr := rtpSender.Read(rtcpBuffer); rtcpErr != nil {
				return
			}
		}
	}()

	return nil
}
