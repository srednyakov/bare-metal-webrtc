package signaling

import (
	"context"
	"io/fs"
	"log"
	"net/http"
	"time"

	"github.com/bare-metal-webrtc/win-streamer-go/internal/config"
	"github.com/gorilla/websocket"
)

// Handler processes a new authenticated WebSocket connection
type Handler interface {
	HandleConnection(ctx context.Context, conn *Connection) error
}

// Server is a minimal HTTP+WebSocket signaling server
type Server struct {
	cfg      config.ServerConfig
	handler  Handler
	httpSvr  *http.Server
	upgrader websocket.Upgrader
}

// New creates a Server. staticFiles is an fs.FS rooted at the static directory
func New(cfg config.ServerConfig, handler Handler, staticFiles fs.FS) *Server {
	s := &Server{
		cfg:     cfg,
		handler: handler,
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool { return true },
		},
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/ws", s.wsHandler)
	mux.Handle("/", http.FileServer(http.FS(staticFiles)))

	s.httpSvr = &http.Server{
		Addr:    cfg.Address,
		Handler: mux,
	}
	return s
}

// ListenAndServe starts the HTTP server and blocks until ctx is cancelled
func (s *Server) ListenAndServe(ctx context.Context) error {
	go func() {
		<-ctx.Done()
		shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer cancel()
		s.httpSvr.Shutdown(shutdownCtx) //nolint:errcheck
	}()
	if err := s.httpSvr.ListenAndServe(); err != http.ErrServerClosed {
		return err
	}
	return nil
}

func (s *Server) wsHandler(w http.ResponseWriter, r *http.Request) {
	wsConn, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("signaling: ws upgrade: %v", err)
		return
	}

	conn := NewConnection(wsConn)
	defer conn.Close()

	if err := s.handler.HandleConnection(r.Context(), conn); err != nil {
		log.Printf("signaling: session: %v", err)
	}
}
