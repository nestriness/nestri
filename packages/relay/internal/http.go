package relay

import (
	"github.com/gorilla/websocket"
	"log"
	"net/http"
	"strconv"
)

var httpMux *http.ServeMux

func InitHTTPEndpoint() {
	// Create HTTP mux which serves our WS endpoint
	httpMux = http.NewServeMux()

	// Endpoints themselves
	httpMux.Handle("/", http.NotFoundHandler())
	httpMux.HandleFunc("/api/ws-ingest/{roomName}", corsAnyHandler(wsIngestHandler))
	httpMux.HandleFunc("/api/ws-participant/{roomName}", corsAnyHandler(wsParticipantHandler))

	// Get our serving port
	port := GetFlags().EndpointPort

	// Log and start the endpoint server
	log.Println("Starting HTTP endpoint server on :", strconv.Itoa(port))
	go func() {
		log.Fatal((&http.Server{
			Handler: httpMux,
			Addr:    ":" + strconv.Itoa(port),
		}).ListenAndServe())
	}()
}

// logHTTPError logs (if verbose) and sends an error code to requester
func logHTTPError(w http.ResponseWriter, err string, code int) {
	if GetFlags().Verbose {
		log.Println(err)
	}
	http.Error(w, err, code)
}

// corsAnyHandler allows any origin to access the endpoint
func corsAnyHandler(next func(w http.ResponseWriter, r *http.Request)) http.HandlerFunc {
	return func(res http.ResponseWriter, req *http.Request) {
		// Allow all origins
		res.Header().Set("Access-Control-Allow-Origin", "*")
		res.Header().Set("Access-Control-Allow-Methods", "*")
		res.Header().Set("Access-Control-Allow-Headers", "*")

		if req.Method != http.MethodOptions {
			next(res, req)
		}
	}
}

// wsIngestHandler is the handler for the /api/ws-ingest/{roomName} endpoint
// takes an "ingest" stream from a nestri-server
func wsIngestHandler(w http.ResponseWriter, r *http.Request) {
	// Get given room name now
	roomName := r.PathValue("roomName")
	if len(roomName) <= 0 {
		logHTTPError(w, "no room name given", http.StatusBadRequest)
		return
	}

	// Check if room by name already exists
	room := GetOrCreateRoom(roomName)
	if room.Online {
		logHTTPError(w, "room name already in use by a node", http.StatusBadRequest)
		return
	}

	// Upgrade to WebSocket
	upgrader := websocket.Upgrader{
		CheckOrigin: func(r *http.Request) bool {
			return true
		},
	}
	wsConn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Assign the WebSocket connection to the room
	room.assignWebSocket(wsConn)

	// If verbose, log
	if GetFlags().Verbose {
		log.Printf("New ingest for room: '%s'\n", room.Name)
	}

	// Start the ingest handler
	go ingestHandler(room)
}

// wsParticipantHandler is the handler for the /api/ws-participant/{roomName} endpoint
// takes a "participant" client connection
func wsParticipantHandler(w http.ResponseWriter, r *http.Request) {
	// Get given room name now
	roomName := r.PathValue("roomName")
	if len(roomName) <= 0 {
		logHTTPError(w, "no room name given", http.StatusBadRequest)
		return
	}
	// Upgrade to WebSocket
	upgrader := websocket.Upgrader{
		CheckOrigin: func(r *http.Request) bool {
			return true
		},
	}
	wsConn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Get room by name
	room := GetOrCreateRoom(roomName)

	// Create new participant
	participant := NewParticipant(wsConn)
	if GetFlags().Verbose {
		log.Printf("New participant: '%s' - for room: '%s'\n", participant.ID, room.Name)
	}

	// Add participant to room
	room.addParticipant(participant)

	// If verbose, log
	if GetFlags().Verbose {
		log.Printf("Added participant: '%s' to room: '%s'\n", participant.ID, room.Name)
	}

	// Start the participant handler
	go participantHandler(participant, room)
}
