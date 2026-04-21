package session

import "github.com/pion/webrtc/v4"

func createPeerConnection(iceServers []webrtc.ICEServer) (*webrtc.PeerConnection, error) {
	return webrtc.NewPeerConnection(webrtc.Configuration{
		ICEServers: iceServers,
	})
}

func createVideoTrack() (*webrtc.TrackLocalStaticSample, error) {
	return webrtc.NewTrackLocalStaticSample(
		webrtc.RTPCodecCapability{MimeType: webrtc.MimeTypeH264},
		"video",
		"screen",
	)
}

// setupRTCPReader drains RTCP so pion's NACK/PLI interceptors stay active
func setupRTCPReader(sender *webrtc.RTPSender) {
	go func() {
		buf := make([]byte, 1500)
		for {
			if _, _, err := sender.Read(buf); err != nil {
				return
			}
		}
	}()
}
