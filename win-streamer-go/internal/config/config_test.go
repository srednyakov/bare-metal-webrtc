package config

import (
	"os"
	"path/filepath"
	"testing"
)

func writeTemp(t *testing.T, content string) string {
	t.Helper()
	f, err := os.CreateTemp(t.TempDir(), "*.yaml")
	if err != nil {
		t.Fatal(err)
	}
	if _, err = f.WriteString(content); err != nil {
		t.Fatal(err)
	}
	f.Close()
	return f.Name()
}

func TestLoadConfig_Full(t *testing.T) {
	path := writeTemp(t, `
output:
  width: 1920
  height: 1080
  fps: 30
x264:
  preset: fast
  profile: main
  rf_constant: 22.0
  threads: 4
server:
  address: "127.0.0.1:9090"
  secret: "victoria"
turn:
  external_ip: "203.0.113.5"
  turn_address: ":5349"
`)
	cfg, err := LoadConfig(path)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	if cfg.Output.Width != 1920 || cfg.Output.Height != 1080 || cfg.Output.FPS != 30 {
		t.Errorf("output mismatch: %+v", cfg.Output)
	}
	if cfg.X264.Preset != "fast" || cfg.X264.Profile != "main" || cfg.X264.RfConstant != 22.0 || cfg.X264.Threads != 4 {
		t.Errorf("x264 mismatch: %+v", cfg.X264)
	}
	if cfg.Server.Address != "127.0.0.1:9090" || cfg.Server.Secret != "victoria" {
		t.Errorf("server mismatch: %+v", cfg.Server)
	}
	if cfg.TURN.ExternalIP != "203.0.113.5" || cfg.TURN.TurnAddress != ":5349" {
		t.Errorf("turn mismatch: %+v", cfg.TURN)
	}
}

func TestLoadConfig_Defaults(t *testing.T) {
	path := writeTemp(t, `server:
  secret: "s3cr3t"
`)
	cfg, err := LoadConfig(path)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	if cfg.Output.FPS != 60 {
		t.Errorf("default FPS: got %d, want 60", cfg.Output.FPS)
	}
	if cfg.X264.Preset != "veryfast" {
		t.Errorf("default preset: got %q, want veryfast", cfg.X264.Preset)
	}
	if cfg.X264.Profile != "high" {
		t.Errorf("default profile: got %q, want high", cfg.X264.Profile)
	}
	if cfg.Server.Address != "0.0.0.0:8080" {
		t.Errorf("default address: got %q, want 0.0.0.0:8080", cfg.Server.Address)
	}
	if cfg.TURN.TurnAddress != ":3478" {
		t.Errorf("default turn_address: got %q, want :3478", cfg.TURN.TurnAddress)
	}
}

func TestLoadConfig_MissingSecret(t *testing.T) {
	path := writeTemp(t, `output:
  fps: 60
`)
	_, err := LoadConfig(path)
	if err == nil {
		t.Fatal("expected error for missing secret, got nil")
	}
}

func TestLoadConfig_FileNotFound(t *testing.T) {
	_, err := LoadConfig(filepath.Join(t.TempDir(), "nonexistent.yaml"))
	if err == nil {
		t.Fatal("expected error for missing file, got nil")
	}
}

func TestLoadConfig_InvalidYAML(t *testing.T) {
	path := writeTemp(t, `output: [bad yaml`)
	_, err := LoadConfig(path)
	if err == nil {
		t.Fatal("expected error for invalid YAML, got nil")
	}
}

func TestTURNConfig_Enabled(t *testing.T) {
	tests := []struct {
		externalIP string
		want       bool
	}{
		{"", false},
		{"203.0.113.5", true},
		{"dns:stream.example.com", true},
	}
	for _, tt := range tests {
		tc := TURNConfig{ExternalIP: tt.externalIP}
		if got := tc.Enabled(); got != tt.want {
			t.Errorf("Enabled(%q) = %v, want %v", tt.externalIP, got, tt.want)
		}
	}
}
