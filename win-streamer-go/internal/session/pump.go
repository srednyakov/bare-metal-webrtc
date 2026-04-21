//go:build windows

package session

import (
	"context"
	"fmt"
	"time"

	"github.com/bare-metal-webrtc/win-streamer-go/internal/bridge"
	"github.com/pion/webrtc/v4"
	"github.com/pion/webrtc/v4/pkg/media"
)

// FramePump pulls H.264 frames from the bridge and writes them to the WebRTC track
type FramePump struct {
	capturer   *bridge.Capturer
	videoTrack *webrtc.TrackLocalStaticSample
	fps        uint32
}

func NewFramePump(capturer *bridge.Capturer, track *webrtc.TrackLocalStaticSample, fps uint32) *FramePump {
	return &FramePump{capturer: capturer, videoTrack: track, fps: fps}
}

// Run blocks until ctx is cancelled. It reads frames via PopFrame()
func (p *FramePump) Run(ctx context.Context) error {
	frameDuration := time.Second / time.Duration(p.fps)
	ticker := time.NewTicker(frameDuration)
	defer ticker.Stop()

	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-ticker.C:
		}

		frame, err := p.capturer.PopFrame()
		if err != nil {
			continue
		}

		if err := p.videoTrack.WriteSample(media.Sample{
			Data:     frame.Data,
			Duration: frameDuration,
		}); err != nil {
			return fmt.Errorf("write sample: %w", err)
		}
	}
}
