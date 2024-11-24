package relay

import (
	"github.com/google/uuid"
	"github.com/pion/webrtc/v4"
	"log"
	"sync"
)

type Room struct {
	ID                uuid.UUID //< Internal IDs are useful to keeping unique internal track
	Name              string
	PeerConnection    *webrtc.PeerConnection
	AudioTrack        webrtc.TrackLocal
	VideoTrack        webrtc.TrackLocal
	DataChannel       *webrtc.DataChannel
	Participants      map[uuid.UUID]*Participant
	ParticipantsMutex sync.Mutex
}

func NewRoom(name string) *Room {
	return &Room{
		ID:           uuid.New(),
		Name:         name,
		Participants: make(map[uuid.UUID]*Participant),
	}
}

// Adds a Participant to a Room
func (r *Room) addParticipant(participant *Participant) {
	r.ParticipantsMutex.Lock()
	r.Participants[participant.ID] = participant
	r.ParticipantsMutex.Unlock()
}

// Removes a Participant from a Room by participant's ID
func (r *Room) removeParticipantByID(pID uuid.UUID) {
	r.ParticipantsMutex.Lock()
	delete(r.Participants, pID)
	r.ParticipantsMutex.Unlock()
}

// Removes a Participant from a Room by participant's name
func (r *Room) removeParticipantByName(pName string) {
	r.ParticipantsMutex.Lock()
	for id, p := range r.Participants {
		if p.Name == pName {
			delete(r.Participants, id)
			break
		}
	}
	r.ParticipantsMutex.Unlock()
}

// Broadcasts a message to Room's Participant's - excluding one given ID of
func (r *Room) broadcastMessage(msg webrtc.DataChannelMessage, excludeID uuid.UUID) {
	for d, participant := range r.Participants {
		if participant.DataChannel != nil {
			if d != excludeID { // Don't send back to the sender
				if err := participant.DataChannel.SendText(string(msg.Data)); err != nil {
					log.Printf("Error broadcasting to %s: %v\n", participant.Name, err)
				}
			}
		}
	}
	if r.DataChannel != nil {
		if err := r.DataChannel.SendText(string(msg.Data)); err != nil {
			log.Printf("Error broadcasting to Room: %v\n", err)
		}
	}
}

// Sends message to Room (nestri-server)
func (r *Room) sendToRoom(msg webrtc.DataChannelMessage) {
	if r.DataChannel != nil {
		if err := r.DataChannel.SendText(string(msg.Data)); err != nil {
			log.Printf("Error broadcasting to Room: %v\n", err)
		}
	}
}
