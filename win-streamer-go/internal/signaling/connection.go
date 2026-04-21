package signaling

import (
	"encoding/json"
	"fmt"
	"sync"

	"github.com/gorilla/websocket"
)

// Connection wraps a gorilla WebSocket with typed read/write and a write mutex
type Connection struct {
	conn *websocket.Conn
	mu   sync.Mutex
}

func NewConnection(conn *websocket.Conn) *Connection {
	return &Connection{conn: conn}
}

// ReadMessage reads one message and returns its "type" field plus the raw JSON
func (c *Connection) ReadMessage() (msgType string, raw json.RawMessage, err error) {
	_, data, err := c.conn.ReadMessage()
	if err != nil {
		return "", nil, err
	}
	var env struct {
		Type string `json:"type"`
	}
	if err := json.Unmarshal(data, &env); err != nil {
		return "", nil, fmt.Errorf("parse message type: %w", err)
	}
	return env.Type, json.RawMessage(data), nil
}

// WriteJSON serialises v to JSON and sends it (thread-safe)
func (c *Connection) WriteJSON(v any) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.conn.WriteJSON(v)
}

// Close closes the underlying WebSocket connection
func (c *Connection) Close() error {
	return c.conn.Close()
}
