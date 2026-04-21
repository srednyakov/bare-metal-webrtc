package config

import (
	"errors"
	"os"

	"gopkg.in/yaml.v3"
)

type Output struct {
	Width  uint32 `yaml:"width"`
	Height uint32 `yaml:"height"`
	FPS    uint32 `yaml:"fps"`
}

type X264 struct {
	Preset     string  `yaml:"preset"`
	Profile    string  `yaml:"profile"`
	RfConstant float32 `yaml:"rf_constant"`
	Threads    uint32  `yaml:"threads"`
}

type ServerConfig struct {
	Address string `yaml:"address"`
	Secret  string `yaml:"secret"`
}

type TURNConfig struct {
	ExternalIP  string `yaml:"external_ip"`
	TurnAddress string `yaml:"turn_address"`
}

// Enabled returns true when a TURN server should be started.
func (t TURNConfig) Enabled() bool {
	return t.ExternalIP != ""
}

// Config is the unified config for both the Go streamer and the C++ capture library.
// The C++ library reads output/x264 and ignores server/turn sections.
type Config struct {
	Output Output       `yaml:"output"`
	X264   X264         `yaml:"x264"`
	Server ServerConfig `yaml:"server"`
	TURN   TURNConfig   `yaml:"turn"`
}

func (c *Config) applyDefaults() {
	if c.Output.FPS == 0 {
		c.Output.FPS = 60
	}
	if c.X264.Preset == "" {
		c.X264.Preset = "veryfast"
	}
	if c.X264.Profile == "" {
		c.X264.Profile = "high"
	}
	if c.Server.Address == "" {
		c.Server.Address = "0.0.0.0:8080"
	}
	if c.TURN.TurnAddress == "" {
		c.TURN.TurnAddress = ":3478"
	}
}

func (c *Config) validate() error {
	if c.Server.Secret == "" {
		return errors.New("server.secret is required")
	}
	return nil
}

func LoadConfig(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	cfg := &Config{}
	if err = yaml.Unmarshal(data, cfg); err != nil {
		return nil, err
	}

	cfg.applyDefaults()

	if err = cfg.validate(); err != nil {
		return nil, err
	}

	return cfg, nil
}
