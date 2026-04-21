//go:build windows

package session

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"sync"

	"github.com/bare-metal-webrtc/win-streamer-go/internal/bridge"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/config"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/signaling"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/turn"
	"github.com/pion/webrtc/v4"
)

// SessionHandler coordinates capture, WebRTC, and signaling for one viewer at a time
type SessionHandler struct {
	capturer   *bridge.Capturer
	turnServer *turn.Server // nil when TURN is disabled
	cfg        config.Config

	mu     sync.Mutex
	active *Session // nil when no session is running
}

// NewHandler creates a handler. turnSvr may be nil for LAN-only mode
func NewHandler(cfg config.Config, turnSvr *turn.Server, capturer *bridge.Capturer) (*SessionHandler, error) {
	return &SessionHandler{
		capturer:   capturer,
		turnServer: turnSvr,
		cfg:        cfg,
	}, nil
}

// HandleConnection implements signaling.Handler
// it authenticates, sets up WebRTC, and runs the session until the browser disconnects.
func (h *SessionHandler) HandleConnection(ctx context.Context, conn *signaling.Connection) error {
	// auth
	msgType, raw, err := conn.ReadMessage()
	if err != nil {
		return fmt.Errorf("read auth: %w", err)
	}
	if msgType != "auth" {
		conn.WriteJSON(signaling.AuthResultMessage{Type: "auth_fail", Reason: "expected auth"}) //nolint:errcheck
		return fmt.Errorf("expected auth, got %q", msgType)
	}
	var authMsg signaling.AuthMessage
	if err := json.Unmarshal(raw, &authMsg); err != nil {
		return fmt.Errorf("parse auth: %w", err)
	}
	if authMsg.Secret != h.cfg.Server.Secret {
		conn.WriteJSON(signaling.AuthResultMessage{Type: "auth_fail", Reason: "invalid secret"}) //nolint:errcheck
		return fmt.Errorf("invalid secret")
	}

	// one-viewer guard
	h.mu.Lock()
	if h.active != nil {
		h.mu.Unlock()
		conn.WriteJSON(signaling.AuthResultMessage{Type: "auth_fail", Reason: "busy"}) //nolint:errcheck
		return fmt.Errorf("session already active")
	}

	// TURN credentials (if enabled)
	var sigICEServers []signaling.ICEServer
	var turnUser string
	if h.turnServer != nil {
		turnUser = generateID()
		password, err := h.turnServer.Credentials(turnUser)
		if err != nil {
			h.mu.Unlock()
			return fmt.Errorf("turn credentials: %w", err)
		}
		sigICEServers = []signaling.ICEServer{{
			URLs:       []string{h.turnServer.Addr()},
			Username:   turnUser,
			Credential: password,
		}}
	}

	// auth_ok
	if err := conn.WriteJSON(signaling.AuthResultMessage{
		Type:       "auth_ok",
		ICEServers: sigICEServers,
	}); err != nil {
		h.mu.Unlock()
		h.disallow(turnUser)
		return fmt.Errorf("send auth_ok: %w", err)
	}

	// create session
	sess, err := newSession(conn, toWebRTCICEServers(sigICEServers), turnUser)
	if err != nil {
		h.mu.Unlock()
		h.disallow(turnUser)
		return fmt.Errorf("create session: %w", err)
	}

	h.active = sess
	h.mu.Unlock()

	defer func() {
		sess.Close()
		h.disallow(turnUser)
		h.mu.Lock()
		h.active = nil
		h.mu.Unlock()
	}()

	// SDP exchange
	if err := sess.negotiate(); err != nil {
		return fmt.Errorf("negotiate: %w", err)
	}

	// run (blocks until done)
	return sess.run(h.capturer, h.cfg.Output.FPS)
}

// Shutdown closes any active session and waits for cleanup
func (h *SessionHandler) Shutdown() {
	h.mu.Lock()
	sess := h.active
	h.mu.Unlock()
	if sess != nil {
		sess.Close()
	}
}

func (h *SessionHandler) disallow(username string) {
	if h.turnServer != nil && username != "" {
		h.turnServer.Disallow(username)
	}
}

func toWebRTCICEServers(servers []signaling.ICEServer) []webrtc.ICEServer {
	out := make([]webrtc.ICEServer, len(servers))
	for i, s := range servers {
		out[i] = webrtc.ICEServer{
			URLs:       s.URLs,
			Username:   s.Username,
			Credential: s.Credential,
		}
	}
	return out
}

func generateID() string {
	buf := make([]byte, 8)
	rand.Read(buf) //nolint:errcheck
	return hex.EncodeToString(buf)
}
