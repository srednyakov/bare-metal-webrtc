# Build Guide

## Prerequisites

### 1. Install MSYS2 with CLANG64

Download: https://www.msys2.org/

### 2. Install Toolchain and Dependencies

```powershell
C:\msys64\msys2_shell.cmd -defterm -here -no-start -clang64 -c "pacman -Syu --noconfirm"
C:\msys64\msys2_shell.cmd -defterm -here -no-start -clang64 -c "pacman -S --noconfirm make mingw-w64-clang-x86_64-toolchain mingw-w64-clang-x86_64-cmake mingw-w64-clang-x86_64-ninja"
```

### 3. Initialize Git Submodules

Pull third-party dependencies (vcpkg, Tracy, SPSCQueue):

```powershell
git submodule update --init --recursive
```

### 4. Install Golang (Optional)

If you don't have Go installed on Windows, you can install it inside the CLANG64 environment:

```powershell
C:\msys64\msys2_shell.cmd -defterm -here -no-start -clang64 -c "pacman -S mingw-w64-clang-x86_64-go"
```

Otherwise, ensure `go` is available in your PATH.

---

## Environment Setup

Before building, set up the CGO environment variables to use CLANG64 compilers.

### PowerShell

```powershell
$env:CGO_ENABLED="1"
$env:CC="clang"
$env:CXX="clang++"
$env:PATH = "C:\msys64\clang64\bin;$env:PATH"
$env:PATH = "C:\msys64\usr\bin;$env:PATH"  # for make
```

### MSYS2 CLANG64 Shell

```bash
export CGO_ENABLED=1
export CC=clang
export CXX=clang++
```

> **Note:** The CLANG64 shell already has the correct PATH and compilers, so you only need to set `CGO_ENABLED`, `CC`, and `CXX`.

---

## Makefile Commands

| Command | Description |
|---------|-------------|
| `make all` | Generate errors + build Release (C++ DLL + Go wrapper) |
| `make build` | Build Release only |
| `make build-debug` | Build Debug (with Tracy profiling) |
| `make gen` | Generate error codes from `errors.json` |
| `make clean` | Remove build artifacts |

### Full Build (Release)

```powershell
make all
```

This will:
1. Generate error code files (C++ header, C++ impl, Go)
2. Build `win-capture-native` DLL (Release)
3. Build `win-streamer-go` wrapper (Release)
4. Copy all outputs to `bin/Release/`

### Debug Build (with Tracy)

```powershell
make build-debug
```

Outputs go to `bin/Debug/` with Tracy profiling enabled.

### Generated Artifacts

After `make all`:

```
bin/Release/
├── libwin-capture-native.dll    # C++ capture library
├── capture-test.exe             # Go test application
└── capture_test_config.yaml     # Example config
```
