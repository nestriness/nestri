package relay

import (
	"fmt"
	"github.com/google/uuid"
	"github.com/pion/webrtc/v4"
	"math/rand"
)

type Participant struct {
	ID             uuid.UUID //< Internal IDs are useful to keeping unique internal track and not have conflicts later
	Name           string
	WebSocket      *SafeWebSocket
	PeerConnection *webrtc.PeerConnection
	DataChannel    *NestriDataChannel
}

func NewParticipant(ws *SafeWebSocket) *Participant {
	return &Participant{
		ID:        uuid.New(),
		Name:      createRandomName(),
		WebSocket: ws,
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

func (vw *Participant) signalOffer() error {
	if vw.PeerConnection == nil {
		return fmt.Errorf("peer connection is nil for participant: '%s' - cannot signal offer", vw.ID)
	}

	offer, err := vw.PeerConnection.CreateOffer(nil)
	if err != nil {
		return err
	}

	err = vw.PeerConnection.SetLocalDescription(offer)
	if err != nil {
		return err
	}

	return vw.WebSocket.SendSDPMessageWS(offer)
}

var namesFirst = []string{"Happy", "Sad", "Angry", "Calm", "Excited", "Bored", "Confused", "Confident", "Curious", "Depressed", "Disappointed", "Embarrassed", "Energetic", "Fearful", "Frustrated", "Glad", "Guilty", "Hopeful", "Impatient", "Jealous", "Lonely", "Motivated", "Nervous", "Optimistic", "Pessimistic", "Proud", "Relaxed", "Shy", "Stressed", "Surprised", "Tired", "Worried"}
var namesSecond = []string{"Dragon", "Unicorn", "Troll", "Goblin", "Elf", "Dwarf", "Ogre", "Gnome", "Mermaid", "Siren", "Vampire", "Ghoul", "Werewolf", "Minotaur", "Centaur", "Griffin", "Phoenix", "Wyvern", "Hydra", "Kraken"}

func createRandomName() string {
	randomFirst := namesFirst[rand.Intn(len(namesFirst))]
	randomSecond := namesSecond[rand.Intn(len(namesSecond))]
	return randomFirst + " " + randomSecond
}
