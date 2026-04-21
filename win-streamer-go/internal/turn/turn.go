package turn

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"net"
	"strings"
	"sync"

	"github.com/bare-metal-webrtc/win-streamer-go/internal/config"
	pionturn "github.com/pion/turn/v4"
)

const realm = "streamer"

// Server wraps a pion TURN server and manages per-session credentials
type Server struct {
	mu   sync.Mutex
	keys map[string][]byte // username -> HMAC-MD5 auth key
	srv  *pionturn.Server
	cfg  config.TURNConfig
	addr string // TURN URL, e.g. "turn:1.2.3.4:3478" — set by Start()
}

// New creates an unstarted Server. Call Start() to bind and begin serving
func New(cfg config.TURNConfig) *Server {
	return &Server{
		keys: make(map[string][]byte),
		cfg:  cfg,
	}
}

// Start resolves external_ip, binds UDP+TCP, and launches the TURN server
func (s *Server) Start() error {
	externalIP, err := resolveExternalIP(s.cfg.ExternalIP)
	if err != nil {
		return fmt.Errorf("turn: %w", err)
	}

	_, port, err := net.SplitHostPort(s.cfg.TurnAddress)
	if err != nil {
		return fmt.Errorf("turn: parse address %q: %w", s.cfg.TurnAddress, err)
	}

	udpConn, err := net.ListenPacket("udp", s.cfg.TurnAddress)
	if err != nil {
		return fmt.Errorf("turn: udp listen: %w", err)
	}

	tcpListener, err := net.Listen("tcp", s.cfg.TurnAddress)
	if err != nil {
		udpConn.Close()
		return fmt.Errorf("turn: tcp listen: %w", err)
	}

	relayGen := &pionturn.RelayAddressGeneratorStatic{
		RelayAddress: externalIP,
		Address:      "0.0.0.0",
	}

	authHandler := func(username, realm string, srcAddr net.Addr) ([]byte, bool) {
		s.mu.Lock()
		key, ok := s.keys[username]
		s.mu.Unlock()
		return key, ok
	}

	srv, err := pionturn.NewServer(pionturn.ServerConfig{
		Realm:       realm,
		AuthHandler: authHandler,
		PacketConnConfigs: []pionturn.PacketConnConfig{
			{PacketConn: udpConn, RelayAddressGenerator: relayGen},
		},
		ListenerConfigs: []pionturn.ListenerConfig{
			{Listener: tcpListener, RelayAddressGenerator: relayGen},
		},
	})
	if err != nil {
		udpConn.Close()
		tcpListener.Close()
		return fmt.Errorf("turn: start server: %w", err)
	}

	s.srv = srv
	s.addr = fmt.Sprintf("turn:%s:%s", externalIP, port)
	return nil
}

// Addr returns the TURN URL (e.g. "turn:1.2.3.4:3478"). Valid after Start()
func (s *Server) Addr() string {
	return s.addr
}

// Credentials generates a random password for username and registers it
// the caller must Disallow(username) on every exit path
func (s *Server) Credentials(username string) (password string, err error) {
	raw := make([]byte, 16)
	if _, err = rand.Read(raw); err != nil {
		return "", fmt.Errorf("turn: generate password: %w", err)
	}
	password = hex.EncodeToString(raw)

	s.mu.Lock()
	s.keys[username] = pionturn.GenerateAuthKey(username, realm, password)
	s.mu.Unlock()
	return password, nil
}

// Disallow revokes TURN credentials for username. Safe to call on missing username
func (s *Server) Disallow(username string) {
	s.mu.Lock()
	delete(s.keys, username)
	s.mu.Unlock()
}

// Close shuts down the TURN server. No-op if Start() was never called
func (s *Server) Close() error {
	if s.srv == nil {
		return nil
	}
	return s.srv.Close()
}

// resolveExternalIP parses a raw IP string or resolves "dns:hostname"
func resolveExternalIP(raw string) (net.IP, error) {
	if strings.HasPrefix(raw, "dns:") {
		hostname := strings.TrimPrefix(raw, "dns:")
		ips, err := net.LookupIP(hostname)
		if err != nil {
			return nil, fmt.Errorf("DNS lookup %q: %w", hostname, err)
		}
		for _, ip := range ips {
			if v4 := ip.To4(); v4 != nil {
				return v4, nil
			}
		}
		if len(ips) == 0 {
			return nil, fmt.Errorf("DNS lookup %q: no results", hostname)
		}
		return ips[0], nil
	}
	ip := net.ParseIP(raw)
	if ip == nil {
		return nil, fmt.Errorf("invalid external_ip %q", raw)
	}
	return ip, nil
}
