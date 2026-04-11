# win-streamer-go

Go streamer based on win-capture-native library with CGO bindings.

## Project Structure

```
win-streamer-go/
├── cmd/
│   └── capture-test/
│       └── main.go                 # Test application (captures frames to file)
├── configs/
│   └── config.example.yaml         # Example YAML configuration
├── internal/
│   ├── bridge/
│   │   ├── capture_bridge.go         # CGO wrapper for win-capture-native C API
│   │   ├── capture_bridge_debug.go   # Debug build tags (profiling, extra logging)
│   │   ├── capture_bridge_release.go # Release build tags (minimal overhead)
│   │   └── capture_error.go          # Generated: CGO error types (from errors.json)
│   └── config/
│       └── config.go               # YAML config loader and validation
├── go.mod                          # Go module definition
├── go.sum                          # Dependency checksums
└── README.md
```

## Build

See [BUILD.md](../BUILD.md) for full instructions.
