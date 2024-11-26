package relay

import (
	"github.com/gorilla/websocket"
	"sync"
)

// SafeWebSocket is a websocket with a mutex
type SafeWebSocket struct {
	*websocket.Conn
	sync.Mutex
}

// NewSafeWebSocket creates a new SafeWebSocket from *websocket.Conn
func NewSafeWebSocket(conn *websocket.Conn) *SafeWebSocket {
	return &SafeWebSocket{
		Conn: conn,
	}
}

// WriteJSON writes JSON to a websocket with a mutex
func (ws *SafeWebSocket) WriteJSON(v interface{}) error {
	ws.Lock()
	defer ws.Unlock()
	return ws.Conn.WriteJSON(v)
}

// WriteBinary writes binary to a websocket with a mutex
func (ws *SafeWebSocket) WriteBinary(data []byte) error {
	ws.Lock()
	defer ws.Unlock()
	return ws.Conn.WriteMessage(websocket.BinaryMessage, data)
}
