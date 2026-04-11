# bare-metal-webrtc

High-performance P2P screen streamer for Windows designed for low latency and optimal resource usage.

> **🎯 Project Goal**  
> This project serves primarily as a **portfolio** demonstrating a powerful combination of Real-Time C++ (capture/encode) and Go (backend logic), and I also hope that this software will be useful to others!

## 🏗️ Architecture

The core C++ capture library (`win-capture-native`) uses a multi-threaded, GPU-accelerated pipeline with lock-free buffers, decoupling heavy graphics processing from application logic. This allows the Go side to pull fully encoded H.264 frames from a shared SPSC queue in less then a microsecond as regular []byte slices.

Combining the best of both worlds: fast development, strong concurrency primitives, and easy WebRTC integration in Go, together with manual resource management and minimal-latency data paths in C++.

📖 See **[win-capture-native/README.md](./win-capture-native/README.md)** for full architecture diagrams, performance metrics.

## 📂 Project Structure

- **[win-capture-native/](./win-capture-native)** - C++ capture library (DXGI, D3D11, x264)
- **[win-streamer-go/](./win-streamer-go)** - Go wrapper and streamer application
- **[codegen/](./codegen)** - Error code generator (JSON → C++/Go)

## 🛠️ Building

See **[BUILD.md](./BUILD.md)** for full build instructions.

---

## 📊 Current Status

The core capture and encoding pipeline is complete and tested.

### ✅ Done

| Component | Task | Description |
|-----------|------|-------------|
| `win-capture-native` | Screen Capture | GPU-accelerated screen capture and color conversion |
| `win-capture-native` | x264 Encoder | CRF encoding, NV12 input, lock-free SPSC queue |
| `win-capture-native` | Tracy Profiling | Built-in performance profiling + smoke-test (optional, zero overhead) |
| `win-capture-native` | Error Recovery | Resolution changes, HDR toggles, GPU driver restarts |
| `win-capture-native` | Build System | CMake presets, MSYS2 CLANG64 environment (no MSVC dependency)  |
| `codegen` | Error Code Generator | `errors.json` → C++ header/impl + Go constants |
| `win-streamer-go` | CGO Bridge | C-style API with Go bindings (`capture_bridge.go`) |
| `win-streamer-go` | Smoke Test | E2E test: capture → encode → file output (60 seconds) |

### ⌛ In Progress / Planned

| Priority | Component | Task | Description |
|----------|-----------|------|-------------|
| **Major** | `win-streamer-go` | WebRTC Backend | P2P signaling, ICE/STUN, media transport (pion/webrtc) |
| **Major** | `win-streamer-go` | Test Coverage | Unit tests for bridge, config, signaling |
| **Major** | `win-capture-native` | Unit Tests | Mocked DXGI/D3D11 tests for buffer logic, config parsing |
| **Major** | `infrastructure` | CI/Ready | GitHub Actions (llvm-mingw build on Linux), tests, release packaging |
| **Major** | `win-capture-native` | NVENC Encoder | Hardware encoding for ultra-low CPU usage |
| **Minor** | `win-capture-native` | Software Adapter Fallback | `libyuv` fallback for VPS/RDP (no hardware Video Processor) |
| **Minor** | `win-streamer-go` | Config Hot-Reload | Runtime resolution/FPS changes without restart |
| **Minor** | `win-capture-native` | Documentation | Add more performance measurement results |

## 📄 License

GPL v3
