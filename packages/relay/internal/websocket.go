package relay

import (
	"fmt"
	"log"
	"net/http"
	"sync"

	"github.com/google/uuid"
	"github.com/gorilla/websocket"
)

// Room represents a chat room
type Room struct {
	participants map[*Participant]bool
	broadcast    chan string
	mutex        sync.RWMutex
	name         string
}

// Participant represents a participant in a chat room
type Participant struct {
	ws   *websocket.Conn
	send chan string
	room *Room
	name string
}

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
}

var rooms = make(map[string]*Room)
var mutex sync.RWMutex

func websocketHandler(w http.ResponseWriter, r *http.Request) {
	roomName := r.PathValue("roomName")
	if len(roomName) <= 0 {
		logHTTPError(w, "no stream name given", http.StatusBadRequest)
		return
	}

	participantName := r.URL.Query().Get("name")
	if participantName == "" {
		participantName = uuid.New().String()
	}

	log.Printf("> New participant %s joining room %s\n", participantName, roomName)

	mutex.Lock()
	room, ok := rooms[roomName]
	if !ok {
		room = &Room{
			participants: make(map[*Participant]bool),
			broadcast:    make(chan string),
			name:         roomName,
		}
		rooms[roomName] = room
		log.Printf("> Created new room %s\n", roomName)
	}
	mutex.Unlock()

	ws, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Println(err)
		return
	}

	participant := &Participant{
		ws:   ws,
		send: make(chan string),
		room: room,
		name: participantName,
	}

	room.mutex.Lock()
	room.participants[participant] = true
	room.mutex.Unlock()

	log.Printf("> Participant %s joined room %s\n", participantName, roomName)

	go participant.writePump()
	go participant.readPump()
	go room.broadcastPump()

}

func (p *Participant) writePump() {
	for message := range p.send {
		err := p.ws.WriteMessage(websocket.TextMessage, []byte(message))
		if err != nil {
			log.Println(err)
			p.room.removeParticipant(p)
			return
		}
	}
}

func (p *Participant) readPump() {
	for {
		_, message, err := p.ws.ReadMessage()
		if err != nil {
			log.Println(err)
			p.room.removeParticipant(p)
			return
		}

		log.Printf("> Participant %s sent message to room %s >>>> %s\n", p.name, p.room.name, string(message))
		p.room.broadcast <- fmt.Sprintf("%s: %s", p.name, string(message))
	}
}

func (r *Room) broadcastPump() {
	for message := range r.broadcast {
		log.Printf("> Broadcasting message to room %s: %s\n", r.name, message)
		r.mutex.RLock()
		for participant := range r.participants {
			participant.send <- message
		}
		r.mutex.RUnlock()
	}
}

func (r *Room) removeParticipant(p *Participant) {
	r.mutex.Lock()
	delete(r.participants, p)
	r.mutex.Unlock()
	close(p.send)
	log.Printf("> Participant %s left room %s\n", p.name, r.name)
}
