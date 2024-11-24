package relay

import (
	"github.com/google/uuid"
	"github.com/pion/webrtc/v4"
	"log"
	"sync"
)

var Rooms = make(map[uuid.UUID]*Room) //< Room ID -> Room
var RoomsMutex = sync.RWMutex{}

func GetRoomByID(id uuid.UUID) *Room {
	RoomsMutex.RLock()
	defer RoomsMutex.RUnlock()
	if room, ok := Rooms[id]; ok {
		return room
	}
	return nil
}

func GetRoomByName(name string) *Room {
	RoomsMutex.RLock()
	defer RoomsMutex.RUnlock()
	for _, room := range Rooms {
		if room.Name == name {
			return room
		}
	}
	return nil
}

func AddRoom(room *Room) {
	if room != nil {
		RoomsMutex.Lock()
		Rooms[room.ID] = room
		RoomsMutex.Unlock()
	}
}

func DeleteRoomIfEmpty(room *Room) {
	if room != nil && !room.Online && len(room.Participants) <= 0 {
		RoomsMutex.Lock()
		delete(Rooms, room.ID)
		RoomsMutex.Unlock()
	}
}

type Room struct {
	ID                uuid.UUID //< Internal IDs are useful to keeping unique internal track
	Name              string
	Online            bool //< Whether the room is currently online, i.e. receiving data from a nestri-server
	PeerConnection    *webrtc.PeerConnection
	AudioTrack        webrtc.TrackLocal
	VideoTrack        webrtc.TrackLocal
	DataChannel       *webrtc.DataChannel
	Participants      map[uuid.UUID]*Participant
	ParticipantsMutex sync.RWMutex
}

func NewRoom(name string) *Room {
	return &Room{
		ID:           uuid.New(),
		Name:         name,
		Online:       false,
		Participants: make(map[uuid.UUID]*Participant),
	}
}

// Adds a Participant to a Room
func (r *Room) addParticipant(participant *Participant) {
	r.ParticipantsMutex.Lock()
	r.Participants[participant.ID] = participant
	r.ParticipantsMutex.Unlock()
}

// Removes a Participant from a Room by participant's ID.
// If Room is offline and this is the last participant, the room is deleted
func (r *Room) removeParticipantByID(pID uuid.UUID) {
	r.ParticipantsMutex.Lock()
	delete(r.Participants, pID)
	r.ParticipantsMutex.Unlock()
	DeleteRoomIfEmpty(r)
}

// Removes a Participant from a Room by participant's name.
// If Room is offline and this is the last participant, the room is deleted
func (r *Room) removeParticipantByName(pName string) {
	r.ParticipantsMutex.Lock()
	for id, p := range r.Participants {
		if p.Name == pName {
			delete(r.Participants, id)
			break
		}
	}
	r.ParticipantsMutex.Unlock()
	DeleteRoomIfEmpty(r)
}

// Broadcasts a message to Room's Participant's - excluding one given ID of
func (r *Room) broadcastMessage(msg webrtc.DataChannelMessage, excludeID uuid.UUID) {
	r.ParticipantsMutex.RLock()
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
	r.ParticipantsMutex.RUnlock()
}

// Sends message to Room (nestri-server)
func (r *Room) sendToRoom(msg webrtc.DataChannelMessage) {
	if r.DataChannel != nil {
		if err := r.DataChannel.SendText(string(msg.Data)); err != nil {
			log.Printf("Error broadcasting to Room: %v\n", err)
		}
	}
}
