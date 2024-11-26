package relay

import (
	"github.com/google/uuid"
	"github.com/gorilla/websocket"
	"github.com/pion/webrtc/v4"
	"math/rand"
)

type Participant struct {
	ID             uuid.UUID //< Internal IDs are useful to keeping unique internal track and not have conflicts later
	Name           string
	WebSocket      *SafeWebSocket
	PeerConnection *webrtc.PeerConnection
	DataChannel    *webrtc.DataChannel
}

func NewParticipant(connection *websocket.Conn) *Participant {
	return &Participant{
		ID:        uuid.New(),
		Name:      createRandomName(),
		WebSocket: NewSafeWebSocket(connection),
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

var namesFirst = []string{"Happy", "Sad", "Angry", "Calm", "Excited", "Bored", "Confused", "Confident", "Curious", "Depressed", "Disappointed", "Embarrassed", "Energetic", "Fearful", "Frustrated", "Glad", "Guilty", "Hopeful", "Impatient", "Jealous", "Lonely", "Motivated", "Nervous", "Optimistic", "Pessimistic", "Proud", "Relaxed", "Shy", "Stressed", "Surprised", "Tired", "Worried"}
var namesSecond = []string{"Dragon", "Unicorn", "Troll", "Goblin", "Elf", "Dwarf", "Ogre", "Gnome", "Mermaid", "Siren", "Vampire", "Ghoul", "Werewolf", "Minotaur", "Centaur", "Griffin", "Phoenix", "Wyvern", "Hydra", "Kraken"}

func createRandomName() string {
	randomFirst := namesFirst[rand.Intn(len(namesFirst))]
	randomSecond := namesSecond[rand.Intn(len(namesSecond))]
	return randomFirst + " " + randomSecond
}
