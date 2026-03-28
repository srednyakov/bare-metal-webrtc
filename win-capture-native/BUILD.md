# Build win-capture-native (Windows target only)

## Requirements

- Visual Studio 2022+ (with C++ workload)
- DirectX SDK & Runtime
- CMake 3.20+
- vcpkg

## Install vcpkg and dependencies

```bash
cd win-capture-native

# clone vcpkg
git submodule update --init --recursive
cd third-party\vcpkg

# bootstrap vcpkg
.\bootstrap-vcpkg.bat

# install dependencies
.\vcpkg install x264:x64-windows yaml-cpp:x64-windows

# (optional) integration with system
.\vcpkg integrate install
```

## Build via CMake

```bash
cd win-capture-native

# CMake config
cmake -B build/ -DCMAKE_TOOLCHAIN_FILE="third-party/vcpkg/scripts/buildsystems/vcpkg.cmake"

# CMake build
cmake --build build/ -j --config Release

# Results:
#   build/bin/Release/win-capture-native.dll
#   ...
```

## Build with Tracy Profiling

To enable performance profiling with [Tracy](https://github.com/wolfpld/tracy):

```bash
cd win-capture-native

# CMake config with profiling enabled
cmake -B build/ -DENABLE_PROFILING=ON -DCMAKE_TOOLCHAIN_FILE="third-party/vcpkg/scripts/buildsystems/vcpkg.cmake"

# CMake build
cmake --build build/ -j --config Release
```

### Run Smoke Test

The smoke test captures 60 seconds of screen and profiles performance:

```bash
# Run smoke test (with Tracy profiling)
cd build/bin/Release/
./win-capture-native-smoke-test.exe

# Output: smoke_output.h264 (encoded video)
```

### Run with Tracy Profiler

1. **Run the smoke test:**
   ```bash
   cd build/bin/Release/
   ./win-capture-native-smoke-test.exe
   ```

2. **Connect Tracy Profiler:**
   - Download [Tracy v0.13.1](https://github.com/wolfpld/tracy/releases/tag/v0.13.1)
   - Run `tracy-profiler.exe` (profiler GUI)
   - Click "Connect" → select `127.0.0.1` → see real-time timeline

3. **Or use Tracy Capture (CLI):**
   ```bash
   tracy-capture.exe -o smoke-test.tracy
   ```
