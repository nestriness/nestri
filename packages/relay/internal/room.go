package relay

import (
	"log"
	"sync"

	"github.com/pion/webrtc/v4"
)

type Room struct {
	PeerConnection        *webrtc.PeerConnection
	AudioTrack            webrtc.TrackLocal
	VideoTrack            webrtc.TrackLocal
	name                  string
	participants          map[string]*Participant
	activeDataChannels    map[*webrtc.DataChannel]string // Participant name keyed by DataChannel for broadcasting
	mutex                 sync.RWMutex
	newParticipantChannel chan *Participant //For adding new participants
}

func (r *Room) listenToParticipants() {
	// Handle new PeerConnections (new participants) as they are established
	go func() {
		for _, participant := range r.participants {
			r.handleParticipantDataChannels(participant)
		}
	}()

	//Listen for new participants being added to the room
	go func() {
		//lint:ignore S1000 ignore this for now
		for {
			select {
			case newParticipant := <-r.newParticipantChannel: // Setup a channel to receive new participants
				r.mutex.Lock()
				r.participants[newParticipant.name] = newParticipant
				r.mutex.Unlock()
				go r.handleParticipantDataChannels(newParticipant)
			}
		}
	}()

	// Main loop to keep this goroutine alive
	select {}
}

func (r *Room) handleParticipantDataChannels(participant *Participant) {
	participant.PeerConnection.OnDataChannel(func(d *webrtc.DataChannel) {
		log.Printf("New DataChannel %s %d\n", d.Label(), d.ID())

		// Register channel opening handling
		d.OnOpen(func() {
			log.Printf("Data channel '%s'-'%d' open.\n", d.Label(), d.ID())
			r.addActiveDataChannel(d, participant.name)
		})

		// Register text message handling
		d.OnMessage(func(msg webrtc.DataChannelMessage) {
			r.broadcastMessage(msg, d) // Exclude the sender
		})

		// Register channel closing handling
		d.OnClose(func() {
			log.Printf("Data channel '%s'-'%d' closed\n", d.Label(), d.ID())
			r.removeActiveDataChannel(d)
		})
	})
}

func (r *Room) addActiveDataChannel(d *webrtc.DataChannel, participantName string) {
	r.mutex.Lock()
	r.activeDataChannels[d] = participantName
	r.mutex.Unlock()
}

func (r *Room) broadcastMessage(msg webrtc.DataChannelMessage, exclude *webrtc.DataChannel) {
	r.mutex.RLock()
	defer r.mutex.RUnlock()
	for d, participantName := range r.activeDataChannels {
		if d != exclude { // Don't send back to the sender
			if err := d.SendText(string(msg.Data)); err != nil {
				log.Printf("Error broadcasting to %s: %v\n", participantName, err)
			}
		}
	}
}

// New method to remove a data channel from the active list
func (r *Room) removeActiveDataChannel(d *webrtc.DataChannel) {
	r.mutex.Lock()
	defer r.mutex.Unlock()
	for channel := range r.activeDataChannels {
		if channel == d {
			delete(r.activeDataChannels, channel)
			log.Printf("Removed channel %s-%d from active data channels\n", d.Label(), d.ID())
			return
		}
	}
	log.Printf("Warning: Attempted to remove non-existent channel %s-%d from active data channels\n", d.Label(), d.ID())
}
