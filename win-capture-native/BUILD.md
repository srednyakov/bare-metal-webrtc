# Build win-capture-native (Windows)

## Requirements

- MSYS2 with CLANG64 environment

## Install Dependencies

1. **Install MSYS2:** https://www.msys2.org/

2. **Install CLANG64 toolchain and dependencies:**
   ```powershell
   C:\msys64\msys2_shell.cmd -defterm -here -no-start -clang64 -c "pacman -Syu --noconfirm"
   C:\msys64\msys2_shell.cmd -defterm -here -no-start -clang64 -c "pacman -S --noconfirm make mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja"

   # OPTIONAL: install Golang in CLANG64 environment (or use Go from Windows)
   C:\msys64\msys2_shell.cmd -defterm -here -no-start -clang64 -c "pacman -S mingw-w64-clang-x86_64-go"
   ```

3. **Install vcpkg via `git submodule update --init --recursive`**

## Build Using CMake Presets

### MSYS2 CLANG64
```bash
cd win-capture-native

# Release build
cmake --preset clang64-release
cmake --build build/ -j

# Debug build (with Tracy profiling)
cmake --preset clang64-debug
cmake --build build/ -j
```

### PowerShell
```powershell
$env:PATH = "C:\msys64\clang64\bin;$env:PATH"
$env:PATH = "C:\msys64\usr\bin;$env:PATH" # for make
cd win-capture-native

# Release build
cmake --preset clang64-release
cmake --build build/ -j

# Debug build (with Tracy profiling)
cmake --preset clang64-debug
cmake --build build/ -j
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENABLE_PROFILING` | OFF | Enable Tracy profiling |

## Run with Tracy Profiling
Connect [tracy-profiler.exe](https://github.com/wolfpld/tracy/releases/tag/v0.13.1) to `127.0.0.1` to view performance timeline.

## Output Structure

```
build/
├── bin/
│   ├── Debug/
│   │   ├── libwin-capture-native.dll
│   │   ├── win-capture-native-smoke-test.exe
│   │   └── smoke_config.yaml
│   └── Release/
│       ├── libwin-capture-native.dll
│       ├── win-capture-native-smoke-test.exe
│       └── smoke_config.yaml
└── lib/
    ├── Debug/
    │   └── libwin-capture-native.dll.a
    └── Release/
        └── libwin-capture-native.dll.a
```
