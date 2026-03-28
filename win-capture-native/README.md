# win-capture-native

High-performance screen capture library for Windows using DXGI Desktop Duplication API and x264 encoder. Delivers ultra-wide 3440×1440@60 FPS with low CPU utilization (tested on AMD Ryzen 5600).

## Features

- **DXGI Desktop Duplication API** - GPU-accelerated screen capture with zero-copy texture handling
- **x264 encoder** - Software H.264 encoding with CRF constant quality mode
- **Lock-free SPSC queue** - Wait-free frame delivery to consumer (Go/external)
- **Thread-pooled architecture** - Separate capture and encoding threads for maximum throughput
- **C-style API** - Clean FFI-friendly interface for cross-language integration
- **YAML configuration** - Runtime-tunable encoding parameters
- **Extensible encoder design** - `IEncoder` interface allows adding new encoders (NVENC, AMF, QSV)
- **GPU color conversion** - Hardware-accelerated conversion (BGRA/RGB10/RGB16 → NV12) via D3D11 Video Processor with HDR10/scRGB support
- **Tracy profiling** - Built-in performance profiling via [Tracy 13.1](https://github.com/wolfpld/tracy/releases/tag/v0.13.1) (optional, zero overhead when disabled)

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          CAPTURE-NATIVE LIBRARY                             │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                       CapturerFabric (Singleton)                     │   │
│  │  - IDXGIFactory1 creation                                            │   │
│  │  - Config loading (YAML)                                             │   │
│  │  - Capturer instantiation                                            │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                      │                                      │
│                                      ▼                                      │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                            Capturer                                  │   │
│  │                                                                      │   │
│  │  ┌────────────────────────────────────────────────────────────────┐  │   │
│  │  │  DXGI Thread (Capturer::Worker)                                │  │   │
│  │  │                                                                │  │   │
│  │  │  AcquireNextFrame → GPU texture (BGRA/RGB10/RGB16)             │  │   │
│  │  │                              │                                 │  │   │
│  │  │                              ▼                                 │  │   │
│  │  │  ┌──────────────────────────────────────────────────────────┐  │  │   │
│  │  │  │  HandleCapturedTexture (in Capturer)                     │  │  │   │
│  │  │  │  - Video Processor: BGRA/RGB10/RGB16 → NV12 (GPU)        │  │  │   │
│  │  │  │  - [optional] CopyResource: NV12 → Staging NV12          │  │  │   │
│  │  │  │  - [optional] Map: CPU read access                       │  │  │   │
│  │  │  └──────────────────────────────────────────────────────────┘  │  │   │
│  │  │                              │                                 │  │   │
│  │  │                              ▼                                 │  │   │
│  │  │  ┌──────────────────────────────────────────────────────────┐  │  │   │
│  │  │  │  CapturedBuffer                                          │  │  │   │
│  │  │  │  - 4 slot ring buffer (lock-free atomics)                │  │  │   │
│  │  │  │  - NV12 format (GPU texture + optional staging)          │  │  │   │
│  │  │  └──────────────────────────────────────────────────────────┘  │  │   │
│  │  └────────────────────────────────────────────────────────────────┘  │   │
│  │                               │                                      │   │
│  │                               ▼                                      │   │
│  │  ┌────────────────────────────────────────────────────────────────┐  │   │
│  │  │  Encoding Thread (IEncoder::Worker)                            │  │   │
│  │  │                                                                │  │   │
│  │  │  ┌──────────────────────────────────────────────────────────┐  │  │   │
│  │  │  │  IEncoder (interface)                                    │  │  │   │
│  │  │  │                                                          │  │  │   │
│  │  │  │  ┌────────────────────┐  ┌────────────────────┐          │  │  │   │
│  │  │  │  │ EncoderX264        │  │ EncoderNVENC       │          │  │  │   │
│  │  │  │  │ - NV12 input       │  │ [FUTURE]           │          │  │  │   │
│  │  │  │  │ - x264 software    │  │ - NVIDIA NVENC HW  │          │  │  │   │
│  │  │  │  │ - CRF mode         │  │ - GPU-accelerated  │          │  │  │   │
│  │  │  │  └────────────────────┘  └────────────────────┘          │  │  │   │
│  │  │  │                                                          │  │  │   │
│  │  │  │  ┌────────────────────────────────────────────────────┐  │  │  │   │
│  │  │  │  │  EncodedBuffer (SPSC Queue, rigtorp)               │  │  │  │   │
│  │  │  │  │  - capacity: 64 frames                             │  │  │  │   │
│  │  │  │  │  - H.264 NAL units                                 │  │  │  │   │
│  │  │  │  └────────────────────────────────────────────────────┘  │  │  │   │
│  │  │  └──────────────────────────────────────────────────────────┘  │  │   │
│  │  └────────────────────────────────────────────────────────────────┘  │   │
│  │                               │                                      │   │
│  └───────────────────────────────┼──────────────────────────────────────┘   │
│                                  │                                          │
│                                  ▼                                          │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         C API (capture.h)                            │   │
│  │  - capture_native_create_capturer_x264()                             │   │
│  │  - capture_native_start_capturer()                                   │   │
│  │  - capture_native_get_frame()  ◄──────────── External Consumer (Go)  │   │
│  │  - capture_native_pop_frame()                                        │   │
│  │  - capture_native_delete_capturer()                                  │   │
│  │                                                                      │   │
│  │  [FUTURE API]                                                        │   │
│  │  - capture_native_create_capturer_nvenc()                            │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

Data Flow:
  DXGI OutputDuplication → GPU Texture (BGRA/RGB10/RGB16)
    → Video Processor (GPU color conversion to NV12)
    → CapturedBuffer (NV12 GPU texture)
    → [if encoder needs CPU] CopyResource → Staging NV12 → Map → x264
    → [if encoder is GPU-based] direct GPU access (future NVENC)
    → H.264 NAL → SPSC Queue → External Consumer

Encoder Implementations:
  • EncoderX264 (current) — NV12 input (CPU-mapped), x264 software encoder, CRF mode
  • EncoderNVENC (future)  — Direct GPU encoding via NVIDIA NVENC API (no CPU copy)
```

## Thread Model

| Thread | Responsibility | Frequency |
|--------|---------------|-----------|
| **DXGI Thread** | `AcquireNextFrame`, Video Processor (BGRA→NV12), `CopyResource` (optional), `Map` (optional) | ~950ms / FPS (slightly faster than target FPS) |
| **Encoding Thread** | `x264_encode`, SPSC push | 1000ms / FPS (exact target FPS) |

The capture thread runs ~5% faster than the encoding thread to ensure fresh frames are always available, preventing encoder starvation.

## Project Structure

```
win-capture-native/
├── CMakeLists.txt              # CMake build configuration (vcpkg, x264, yaml-cpp)
├── BUILD.md                    # Build instructions
├── README.md                   # This file
│
├── include/                    # Public headers
│   ├── capture.h               # C-style API (FFI-friendly)
│   ├── types.h                 # Public types (EncodedFrame, CaptureError)
│   └── internal/               # Internal C++ headers
│       ├── capturer.hpp        # Main Capturer class (DXGI thread, Video Processor)
│       ├── capturer_fabric.hpp # Singleton factory
│       ├── encoder.hpp         # IEncoder interface
│       ├── config.hpp          # YAML config structure
│       ├── config-nodes/
│       │   ├── general.hpp     # General settings (width, height, fps)
│       │   └── x264.hpp        # x264 settings (preset, CRF, threads)
│       ├── encoders/
│       │   ├── encoder_x264.hpp  # x264 software encoder (NV12 input)
│       │   └── encoder_nvenc.hpp # [FUTURE] NVIDIA NVENC hardware encoder
│       ├── buffers/
│       │   ├── captured_buffer.hpp # Lock-free ring buffer (4 slots, NV12)
│       │   └── encoded_buffer.hpp  # SPSC queue wrapper
│       └── utils/
│           └── scope_exit.hpp  # RAII scope guard
│           └── timer.hpp       # RAII WaitableTimer
│
├── src/                        # Implementation
│   ├── capture.cpp             # C API wrappers
│   └── internal/
│       ├── capturer.cpp        # Capturer implementation (GPU color conversion)
│       ├── capturer_fabric.cpp # Fabric singleton
│       ├── encoders/
│       │   ├── encoder_x264.cpp    # x264 encoding (NV12 CPU input)
│       │   └── encoder_nvenc.cpp   # [FUTURE] NVENC hardware encoding
│       └── buffers/
│           └── captured_buffer.cpp # Ring buffer lock/unlock logic
│
│
├── tests/                      # Smoke tests with Tracy profiling
│   ├── smoke_test.cpp          # E2E test (30 seconds capture)
│   ├── smoke_config.yaml       # Test configuration
│   └── CMakeLists.txt
│
└── third-party/
    ├── vcpkg/                  # vcpkg submodule (x264, yaml-cpp)
    ├── SPSCQueue/              # rigtorp/SPSCQueue (lock-free queue)
    └── tracy/                  # wolfpld/tracy profiler (v0.13.1)
```

## File Descriptions

### Core Components

| File | Description |
|------|-------------|
| `include/capture.h` | **Public C API** - `create_capturer`, `start`, `get_frame`, `pop_frame`, `delete`. Designed for FFI (Go/Python/Rust). |
| `include/types.h` | **Public types** - `EncodedFrame` (data/size/pts/keyframe), `CaptureError` enum (28 error codes). |
| `include/internal/capturer.hpp` | **DXGI thread owner** - Manages `IDXGIOutputDuplication`, D3D11 device/context, Video Processor for GPU color conversion (BGRA/RGB10/RGB16 → NV12). |
| `include/internal/encoder.hpp` | **IEncoder interface** - Base class with `CapturedBuffer`, `EncodedBuffer`, worker thread. Designed for extension (NVENC, AMF, QSV). |
| `include/internal/encoders/encoder_x264.hpp` | **x264 implementation** - NV12 input (CPU-mapped), x264 CRF encoding, SPSC push. |
| `include/internal/encoders/encoder_nvenc.hpp` | **[FUTURE]** - NVIDIA NVENC hardware encoder (direct GPU encoding, no CPU copy). |
| `include/internal/buffers/captured_buffer.hpp` | **Lock-free ring buffer** - 4 slots with atomic lock/index. Stores NV12 textures (GPU + optional staging). |
| `include/internal/buffers/encoded_buffer.hpp` | **SPSC queue wrapper** - rigtorp::SPSCQueue<EncodedFrameInternal>, capacity 64. |
| `include/internal/capturer_fabric.hpp` | **Singleton factory** - Creates `IDXGIFactory1`, loads YAML config, instantiates Capturer. |
| `include/internal/config.hpp` | **Configuration** - YAML-decodable struct with `general` (width/height/fps) and `x264` (preset/CRF/threads). |

### Implementation Files

| File | Description |
|------|-------------|
| `src/capture.cpp` | **C API implementation** - Wraps C++ classes, handles pointer casting, error conversion. |
| `src/internal/capturer.cpp` | **DXGI capture logic** - `AcquireNextFrame`, Video Processor initialization, GPU color conversion (BGRA/RGB10/RGB16 → NV12), `SelectColorSpace` for HDR10/scRGB support. |
| `src/internal/encoder_x264.cpp` | **Encoding pipeline** - NV12 texture mapping, x264 encode, NAL assembly, SPSC push. |
| `src/internal/encoder_nvenc.cpp` | **[FUTURE]** - NVENC hardware encoding pipeline (zero-copy GPU path). |
| `src/internal/buffers/captured_buffer.cpp` | **Slot management** - `TryLockLatestSlot` (for read), `TryLockOldestSlot` (for write), atomic compare-exchange. |

### Utilities

| File | Description |
|------|-------------|
| `include/internal/utils/scope_exit.hpp` | **RAII guard** - `scope_exit` for `ReleaseFrame`, `Unlock`, `Unmap` cleanup. |
| `include/internal/config-nodes/*.hpp` | **YAML parsers** - `yaml-cpp` specializations for `General` and `X264` structs. |

## Usage Example (C++)

```cpp
#include "capture.h"

void* capturer;
capture_native_create_capturer_x264("config.yaml", &capturer);
capture_native_start_capturer(capturer);

while (running) {
    timer.Wait(1s / fps);
    EncodedFrame frame = capture_native_get_frame(capturer);
    if (frame.data != nullptr) {
        // Process H.264 NAL unit
        send_to_webrtc(frame.data, frame.size, frame.pts, frame.keyframe);
        capture_native_pop_frame(capturer);
    }
}

capture_native_delete_capturer(capturer);
```

## Testing

### Smoke Test (with Tracy Profiling)

The smoke test validates capture functionality and collects performance metrics:

```bash
# Build with profiling
cmake -B build -DENABLE_PROFILING=ON
cmake --build build --config Release

# Run smoke test (30 seconds capture)
./build/bin/Release/win-capture-native-smoke-test.exe

# Output: smoke_output.h264
```

Connect [Tracy v0.13.1](https://github.com/wolfpld/tracy/releases/tag/v0.13.1) for real-time performance timeline.

### Project Structure

```
win-capture-native/
├── tests/
│   ├── smoke_test.cpp        # E2E test with Tracy profiling
│   ├── smoke_config.yaml     # Test configuration
│   └── CMakeLists.txt
```

## Configuration (YAML)

```yaml
general:
  width: 3440
  height: 1440
  fps: 60

x264:
  preset: veryfast    # ultrafast, superfast, veryfast, faster, fast
  profile: high       # baseline, main, high
  rf_constant: 18.0   # 0-51 (lower = better quality, higher = smaller size)
  threads: 0          # 0 = auto, -1 = sync lookahead
```

## Performance X264 (Tracy profile)

CPU: AMD Ryzen 5 5600 (threads: X264_THREADS_AUTO, profile: high, rf_constant: 0.0)
| Resolution | FPS | Encode Time | Preset    | 
|------------|-----|-------------|-----------|
| 3440×1440  | 60  | ~3.5ms      | ultrafast |
| 3440×1440  | 60  | ~6.9ms      | veryfast  |
| 3440×1440  | 60  | ~12.9ms     | fast      |

## Error Handling

All errors are defined in `types.h` as `CaptureError`:

Retrieve errors via:
- `capture_native_get_last_capturer_error()` - DXGI thread errors
- `capture_native_get_last_encoder_error()` - Encoding thread errors

## Dependencies

- **x264** - H.264 software encoding (via vcpkg)
- **yaml-cpp** - YAML configuration parsing (via vcpkg)
- **rigtorp/SPSCQueue** - Lock-free SPSC queue (submodule)
- **DirectX SDK** - DXGI, D3D11, D3D11 Video Processor (included in Windows SDK)
- **Tracy** (optional) - Performance profiler (submodule, zero overhead when `ENABLE_PROFILING=OFF`)

## License

GPL v3
