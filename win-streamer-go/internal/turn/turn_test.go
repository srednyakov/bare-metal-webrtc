package turn

import (
	"fmt"
	"net"
	"testing"

	"github.com/bare-metal-webrtc/win-streamer-go/internal/config"
	pionturn "github.com/pion/turn/v4"
)

func newUnstarted() *Server {
	return New(config.TURNConfig{
		ExternalIP:  "127.0.0.1",
		TurnAddress: ":3478",
	})
}

// --- Credential lifecycle (no network) ---

func TestCredentials_ReturnsNonEmptyPassword(t *testing.T) {
	s := newUnstarted()
	pw, err := s.Credentials("user1")
	if err != nil {
		t.Fatal(err)
	}
	if pw == "" {
		t.Fatal("expected non-empty password")
	}
}

func TestCredentials_StoresAuthKey(t *testing.T) {
	s := newUnstarted()
	pw, err := s.Credentials("user1")
	if err != nil {
		t.Fatal(err)
	}

	s.mu.Lock()
	key, ok := s.keys["user1"]
	s.mu.Unlock()

	if !ok {
		t.Fatal("expected key to be stored after Credentials()")
	}
	want := pionturn.GenerateAuthKey("user1", realm, pw)
	if string(key) != string(want) {
		t.Fatalf("stored key mismatch: got %x want %x", key, want)
	}
}

func TestDisallow_RemovesKey(t *testing.T) {
	s := newUnstarted()
	if _, err := s.Credentials("user1"); err != nil {
		t.Fatal(err)
	}

	s.Disallow("user1")

	s.mu.Lock()
	_, ok := s.keys["user1"]
	s.mu.Unlock()

	if ok {
		t.Fatal("expected key to be removed after Disallow()")
	}
}

func TestDisallow_NoopForMissingUsername(t *testing.T) {
	s := newUnstarted()
	s.Disallow("ghost") // must not panic
}

func TestCredentials_EachCallGeneratesDifferentPassword(t *testing.T) {
	s := newUnstarted()
	pw1, _ := s.Credentials("user1")
	pw2, _ := s.Credentials("user1")
	if pw1 == pw2 {
		t.Fatal("expected different passwords on subsequent Credentials() calls")
	}
}

func TestCredentials_IndependentPerUser(t *testing.T) {
	s := newUnstarted()
	_, err1 := s.Credentials("alice")
	_, err2 := s.Credentials("bob")
	if err1 != nil || err2 != nil {
		t.Fatalf("unexpected errors: %v %v", err1, err2)
	}

	s.Disallow("alice")

	s.mu.Lock()
	_, aliceOK := s.keys["alice"]
	_, bobOK := s.keys["bob"]
	s.mu.Unlock()

	if aliceOK {
		t.Fatal("alice's key should be removed")
	}
	if !bobOK {
		t.Fatal("bob's key should still be present")
	}
}

// --- Integration: Start / Close ---

func freeUDPPort(t *testing.T) int {
	t.Helper()
	conn, err := net.ListenUDP("udp", &net.UDPAddr{IP: net.ParseIP("127.0.0.1"), Port: 0})
	if err != nil {
		t.Fatalf("find free UDP port: %v", err)
	}
	port := conn.LocalAddr().(*net.UDPAddr).Port
	conn.Close()
	return port
}

func TestServer_StartClose(t *testing.T) {
	port := freeUDPPort(t)
	s := New(config.TURNConfig{
		ExternalIP:  "127.0.0.1",
		TurnAddress: fmt.Sprintf(":%d", port),
	})
	if err := s.Start(); err != nil {
		t.Fatalf("Start: %v", err)
	}
	if err := s.Close(); err != nil {
		t.Fatalf("Close: %v", err)
	}
}

func TestStart_InvalidExternalIP(t *testing.T) {
	s := New(config.TURNConfig{
		ExternalIP:  "not-an-ip",
		TurnAddress: ":3478",
	})
	if err := s.Start(); err == nil {
		t.Fatal("expected error for invalid external_ip")
	}
}

func TestStart_InvalidAddress(t *testing.T) {
	s := New(config.TURNConfig{
		ExternalIP:  "127.0.0.1",
		TurnAddress: ":::bad",
	})
	if err := s.Start(); err == nil {
		t.Fatal("expected error for invalid turn_address")
	}
}

func TestClose_BeforeStart(t *testing.T) {
	s := newUnstarted()
	if err := s.Close(); err != nil {
		t.Fatalf("Close before Start should be a no-op, got: %v", err)
	}
}
