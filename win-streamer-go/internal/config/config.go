package config

import (
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

type Config struct {
	Output Output `yaml:"output"`
	X264   X264   `yaml:"x264"`
}

func LoadConfig(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	cfg := &Config{}
	err = yaml.Unmarshal(data, cfg)
	if err != nil {
		return nil, err
	}

	return cfg, nil
}
