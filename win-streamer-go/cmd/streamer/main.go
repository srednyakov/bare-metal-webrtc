//go:build windows

package main

import (
	"context"
	"fmt"
	"io/fs"
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/bare-metal-webrtc/win-streamer-go/internal/bridge"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/config"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/session"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/signaling"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/turn"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/web"
)

func main() {
	configPath := "streamer.yaml"
	if len(os.Args) > 1 {
		configPath = os.Args[1]
	}

	cfg, err := config.LoadConfig(configPath)
	if err != nil {
		log.Fatalf("config: %v", err)
	}

	capturer, err := bridge.NewCapturerX264(configPath)
	if err != nil {
		log.Fatalf("capturer: %v", err)
	}
	defer capturer.Close()

	if err := capturer.Start(); err != nil {
		log.Fatalf("capturer start: %v", err)
	}
	defer capturer.Stop()

	var turnSvr *turn.Server
	if cfg.TURN.Enabled() {
		turnSvr = turn.New(cfg.TURN)
		if err := turnSvr.Start(); err != nil {
			log.Fatalf("TURN: %v", err)
		}
		defer turnSvr.Close()
		fmt.Printf("TURN:    %s\n", turnSvr.Addr())
	}

	handler, err := session.NewHandler(*cfg, turnSvr, capturer)
	if err != nil {
		log.Fatalf("handler: %v", err)
	}

	staticFS, err := fs.Sub(web.StaticFS, "static")
	if err != nil {
		log.Fatalf("static fs: %v", err)
	}

	srv := signaling.New(cfg.Server, handler, staticFS)

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer stop()

	fmt.Printf("Listening: http://%s\n", cfg.Server.Address)
	if err := srv.ListenAndServe(ctx); err != nil {
		log.Printf("server: %v", err)
	}

	handler.Shutdown()
	fmt.Println("Shutdown complete.")
}
