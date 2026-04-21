'use strict';

const statusEl    = document.getElementById('status');
const authSection = document.getElementById('auth-section');
const videoSection = document.getElementById('video-section');
const videoEl     = document.getElementById('video');
const authForm    = document.getElementById('auth-form');
const secretInput = document.getElementById('secret');

function setStatus(msg) { statusEl.textContent = msg; }

const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
const ws = new WebSocket(`${proto}//${location.host}/ws`);

ws.onopen = () => setStatus('Connected. Enter secret.');

ws.onerror = () => setStatus('WebSocket error.');

ws.onclose = () => setStatus('Disconnected.');

let pc = null;
let iceServers = [];

ws.onmessage = async (event) => {
  let msg;
  try { msg = JSON.parse(event.data); } catch { return; }

  switch (msg.type) {

    case 'auth_ok':
      iceServers = msg.iceServers || [];
      setStatus('Authenticated — waiting for offer…');
      break;

    case 'auth_fail':
      setStatus(`Auth failed: ${msg.reason || 'unknown'}`);
      break;

    case 'offer':
      authSection.style.display = 'none';
      videoSection.style.display = 'block';
      setStatus('Connecting…');

      pc = new RTCPeerConnection({ iceServers });

      pc.onicecandidate = ({ candidate }) => {
        if (candidate) ws.send(JSON.stringify({
          type: 'candidate',
          candidate: {
            candidate:        candidate.candidate,
            sdpMid:           candidate.sdpMid,
            sdpMLineIndex:    candidate.sdpMLineIndex,
            usernameFragment: candidate.usernameFragment,
          },
        }));
      };

      pc.onconnectionstatechange = () => {
        setStatus(pc.connectionState);
      };

      pc.ontrack = ({ streams }) => {
        videoEl.srcObject = streams[0];
      };

      await pc.setRemoteDescription(msg.sdp);
      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);
      // Explicitly extract type/sdp (Firefox RTCSessionDescription properties)
      // are WebIDL prototype attributes, not own enumerable props, so JSON.stringify(pc.localDescription) produces {} in Firefox.
      ws.send(JSON.stringify({ type: 'answer', sdp: { type: answer.type, sdp: answer.sdp } }));
      break;

    case 'candidate':
      if (pc && msg.candidate) {
        try { await pc.addIceCandidate(msg.candidate); } catch { /* ignore */ }
      }
      break;
  }
};

authForm.addEventListener('submit', (e) => {
  e.preventDefault();
  const secret = secretInput.value.trim();
  if (!secret) return;
  setStatus('Authenticating…');
  ws.send(JSON.stringify({ type: 'auth', secret }));
});
