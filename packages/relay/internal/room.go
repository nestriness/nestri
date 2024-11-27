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

func GetOrCreateRoom(name string) *Room {
	if room := GetRoomByName(name); room != nil {
		return room
	}
	RoomsMutex.Lock()
	room := NewRoom(name)
	Rooms[room.ID] = room
	if GetFlags().Verbose {
		log.Printf("New room: '%s'\n", room.Name)
	}
	RoomsMutex.Unlock()
	return room
}

func DeleteRoomIfEmpty(room *Room) {
	room.ParticipantsMutex.RLock()
	defer room.ParticipantsMutex.RUnlock()
	if !room.Online && len(room.Participants) <= 0 {
		RoomsMutex.Lock()
		delete(Rooms, room.ID)
		RoomsMutex.Unlock()
	}
}

type Room struct {
	ID                uuid.UUID //< Internal IDs are useful to keeping unique internal track
	Name              string
	Online            bool //< Whether the room is currently online, i.e. receiving data from a nestri-server
	WebSocket         *SafeWebSocket
	PeerConnection    *webrtc.PeerConnection
	AudioTrack        webrtc.TrackLocal
	VideoTrack        webrtc.TrackLocal
	DataChannel       *NestriDataChannel
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

// Assigns a WebSocket connection to a Room
func (r *Room) assignWebSocket(ws *SafeWebSocket) {
	// If WS already assigned, warn
	if r.WebSocket != nil {
		log.Printf("Warning: Room '%s' already has a WebSocket assigned\n", r.Name)
	}
	r.WebSocket = ws
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

// Signals all participants with offer and add tracks to their PeerConnections
func (r *Room) signalParticipantsWithTracks() {
	r.ParticipantsMutex.RLock()
	for _, participant := range r.Participants {
		// Add tracks to participant's PeerConnection
		if r.AudioTrack != nil {
			if err := participant.addTrack(&r.AudioTrack); err != nil {
				log.Printf("Failed to add audio track to participant: '%s' - reason: %s\n", participant.ID, err)
			}
		}
		if r.VideoTrack != nil {
			if err := participant.addTrack(&r.VideoTrack); err != nil {
				log.Printf("Failed to add video track to participant: '%s' - reason: %s\n", participant.ID, err)
			}
		}
		// Signal participant with offer
		if err := participant.signalOffer(); err != nil {
			log.Printf("Error signaling participant: %v\n", err)
		}
	}
	r.ParticipantsMutex.RUnlock()
}

// Signals all participants that the Room is offline
func (r *Room) signalParticipantsOffline() {
	r.ParticipantsMutex.RLock()
	for _, participant := range r.Participants {
		if err := participant.WebSocket.SendAnswerMessageWS(AnswerOffline); err != nil {
			log.Printf("Failed to send Offline answer for participant: '%s' - reason: %s\n", participant.ID, err)
		}
	}
	r.ParticipantsMutex.RUnlock()
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
