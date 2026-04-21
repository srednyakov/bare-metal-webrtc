package signaling

// --- Incoming from browser ---

type AuthMessage struct {
	Type   string `json:"type"`
	Secret string `json:"secret"`
}

type SDPPayload struct {
	Type string `json:"type"`
	SDP  string `json:"sdp"`
}

type ICEPayload struct {
	Candidate        string  `json:"candidate"`
	SDPMid           *string `json:"sdpMid"`
	SDPMLineIndex    *uint16 `json:"sdpMLineIndex"`
	UsernameFragment *string `json:"usernameFragment,omitempty"`
}

type AnswerMessage struct {
	Type string     `json:"type"`
	SDP  SDPPayload `json:"sdp"`
}

type CandidateMessage struct {
	Type      string     `json:"type"`
	Candidate ICEPayload `json:"candidate"`
}

// --- Outgoing from server ---

type ICEServer struct {
	URLs       []string `json:"urls"`
	Username   string   `json:"username,omitempty"`
	Credential string   `json:"credential,omitempty"`
}

type AuthResultMessage struct {
	Type       string      `json:"type"`             // "auth_ok" | "auth_fail"
	Reason     string      `json:"reason,omitempty"` // set on auth_fail
	ICEServers []ICEServer `json:"iceServers,omitempty"`
}

type OfferMessage struct {
	Type string     `json:"type"` // "offer"
	SDP  SDPPayload `json:"sdp"`
}

type ServerCandidateMessage struct {
	Type      string     `json:"type"` // "candidate"
	Candidate ICEPayload `json:"candidate"`
}
