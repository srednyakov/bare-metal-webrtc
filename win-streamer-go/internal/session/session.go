//go:build windows

package session

import (
	"context"
	"encoding/json"
	"fmt"
	"log"

	"github.com/bare-metal-webrtc/win-streamer-go/internal/bridge"
	"github.com/bare-metal-webrtc/win-streamer-go/internal/signaling"
	"github.com/pion/webrtc/v4"
)

// Session owns one active P2P WebRTC connection
type Session struct {
	pc         *webrtc.PeerConnection
	videoTrack *webrtc.TrackLocalStaticSample
	conn       *signaling.Connection
	turnUser   string

	ctx    context.Context
	cancel context.CancelFunc
}

func newSession(conn *signaling.Connection, iceServers []webrtc.ICEServer, turnUser string) (*Session, error) {
	ctx, cancel := context.WithCancel(context.Background())

	pc, err := createPeerConnection(iceServers)
	if err != nil {
		cancel()
		return nil, fmt.Errorf("peer connection: %w", err)
	}

	videoTrack, err := createVideoTrack()
	if err != nil {
		cancel()
		pc.Close()
		return nil, fmt.Errorf("video track: %w", err)
	}

	sender, err := pc.AddTrack(videoTrack)
	if err != nil {
		cancel()
		pc.Close()
		return nil, fmt.Errorf("add track: %w", err)
	}
	setupRTCPReader(sender)

	sess := &Session{
		pc:         pc,
		videoTrack: videoTrack,
		conn:       conn,
		turnUser:   turnUser,
		ctx:        ctx,
		cancel:     cancel,
	}

	// OnICECandidate runs in a pion goroutine (only WriteJSON, no mu.Lock)
	pc.OnICECandidate(func(c *webrtc.ICECandidate) {
		if c == nil {
			log.Printf("session: ICE gathering complete (server side)")
			return
		}
		log.Printf("session: server ICE candidate: %s", c.String())
		init := c.ToJSON()
		conn.WriteJSON(signaling.ServerCandidateMessage{ //nolint:errcheck
			Type: "candidate",
			Candidate: signaling.ICEPayload{
				Candidate:        init.Candidate,
				SDPMid:           init.SDPMid,
				SDPMLineIndex:    init.SDPMLineIndex,
				UsernameFragment: init.UsernameFragment,
			},
		})
	})

	return sess, nil
}

// negotiate performs the SDP offer/answer exchange
func (s *Session) negotiate() error {
	offer, err := s.pc.CreateOffer(nil)
	if err != nil {
		return fmt.Errorf("create offer: %w", err)
	}
	if err := s.pc.SetLocalDescription(offer); err != nil {
		return fmt.Errorf("set local description: %w", err)
	}
	if err := s.conn.WriteJSON(signaling.OfferMessage{
		Type: "offer",
		SDP:  signaling.SDPPayload{Type: offer.Type.String(), SDP: offer.SDP},
	}); err != nil {
		return fmt.Errorf("send offer: %w", err)
	}

	msgType, raw, err := s.conn.ReadMessage()
	if err != nil {
		return fmt.Errorf("read answer: %w", err)
	}
	if msgType != "answer" {
		return fmt.Errorf("expected answer, got %q", msgType)
	}
	var msg signaling.AnswerMessage
	if err := json.Unmarshal(raw, &msg); err != nil {
		return fmt.Errorf("parse answer: %w", err)
	}
	log.Printf("session: answer sdp type=%q len=%d", msg.SDP.Type, len(msg.SDP.SDP))
	return s.pc.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.NewSDPType(msg.SDP.Type),
		SDP:  msg.SDP.SDP,
	})
}

// run starts the frame pump immediately and drains incoming ICE candidates
func (s *Session) run(capturer *bridge.Capturer, fps uint32) error {
	pumpCh := make(chan error, 1)

	pump := NewFramePump(capturer, s.videoTrack, fps)
	go func() { pumpCh <- pump.Run(s.ctx) }()
	log.Printf("session: frame pump started (%d fps)", fps)

	s.pc.OnICEConnectionStateChange(func(state webrtc.ICEConnectionState) {
		log.Printf("session: ICE connection state → %s", state)
	})

	s.pc.OnConnectionStateChange(func(state webrtc.PeerConnectionState) {
		log.Printf("session: connection state → %s", state)
		switch state {
		case webrtc.PeerConnectionStateFailed, webrtc.PeerConnectionStateClosed:
			s.cancel()
		}
	})

	// read ICE candidates from browser until connection closes
	go func() {
		for {
			msgType, raw, err := s.conn.ReadMessage()
			if err != nil {
				s.cancel()
				return
			}
			if msgType != "candidate" {
				continue
			}
			var msg signaling.CandidateMessage
			if err := json.Unmarshal(raw, &msg); err != nil {
				continue
			}
			log.Printf("session: browser ICE candidate: %s", msg.Candidate.Candidate)
			s.pc.AddICECandidate(webrtc.ICECandidateInit{ //nolint:errcheck
				Candidate:        msg.Candidate.Candidate,
				SDPMid:           msg.Candidate.SDPMid,
				SDPMLineIndex:    msg.Candidate.SDPMLineIndex,
				UsernameFragment: msg.Candidate.UsernameFragment,
			})
		}
	}()

	select {
	case err := <-pumpCh:
		return err
	case <-s.ctx.Done():
		return nil
	}
}

// Close cancels the session context and closes the PeerConnection and WebSocket
func (s *Session) Close() error {
	s.cancel()
	err := s.pc.Close()
	s.conn.Close() //nolint:errcheck
	return err
}
