//go:build windows
// +build windows

package main

import (
	"fmt"
	"log"
	"os"
	"time"

	"github.com/bare-metal-webrtc/win-streamer-go/internal/bridge"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/config"
)

const (
	expectedPts = 3600 // 60 seconds at 60FPS
	slackFactor = 98   // 98% of expected period
)

func main() {
	const configPath = "capture_test_config.yaml"
	const outputFile = "capture_test_output.h264"

	fmt.Printf("Creating capturer with config: %s\n", configPath)

	cfg, err := config.LoadConfig(configPath)
	if err != nil {
		log.Fatalf("Failed to load config: %v\n", err)
	}

	frameInterval := (time.Second * time.Duration(slackFactor)) / (100 * time.Duration(cfg.Output.FPS))

	capturer, err := bridge.NewCapturerX264(configPath)
	if err != nil {
		log.Fatalf("Failed to create capturer: %v\n", err)
	}
	defer capturer.Close()

	fmt.Println("Starting capturer...")
	if err := capturer.Start(); err != nil {
		log.Fatalf("Failed to start capturer: %v\n", err)
	}
	defer capturer.Stop()

	file, err := os.Create(outputFile)
	if err != nil {
		log.Fatalf("Failed to create output file: %v\n", err)
	}
	defer file.Close()

	fmt.Printf("Capturing %d frames to %s as %dx%d@%d...\n",
		expectedPts, outputFile, cfg.Output.Width, cfg.Output.Height, cfg.Output.FPS)

	ticker := time.NewTicker(frameInterval)
	defer ticker.Stop()

	pts := uint64(0)
	startTime := time.Now()

	for pts < expectedPts {
		<-ticker.C

		frame, err := capturer.PopFrame()
		if err != nil {
			capturerErr := capturer.GetLastCapturerError()
			encoderErr := capturer.GetLastEncoderError()
			if capturerErr != bridge.CaptureErrorOK || encoderErr != bridge.CaptureErrorOK {
				fmt.Printf("Errors - Capturer: %v, Encoder: %v\n", capturerErr, encoderErr)
			}
			continue
		}

		_, err = file.Write(frame.Data)
		if err != nil {
			fmt.Printf("Failed to write frame: %v\n", err)
			break
		}

		pts = frame.Pts

		if pts != 0 && pts%60 == 0 {
			elapsed := time.Since(startTime).Milliseconds()
			fmt.Printf("Done %d/%d (%dms)\n", pts, expectedPts, elapsed)
			startTime = time.Now()
		}
	}

	fmt.Println("Capture complete!")
}
