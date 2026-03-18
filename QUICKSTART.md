# Quick Start Guide

## Current Status

✅ **Project Structure Created**
- Source files for Viam camera component
- CMake build configuration
- Makefile for easy building
- Ensenso SDK is installed

❌ **Need to Install: Viam C++ SDK**

## Next Steps

### 1. Install Viam C++ SDK

Choose one of these options:

**Option A: Docker Build (Recommended)**
```bash
# This will build the module inside a Docker container with all dependencies
docker pull ghcr.io/viamrobotics/viam-cpp-sdk:latest
docker run --rm -v $(pwd):/workspace -w /workspace \
  -v /opt/ensenso:/opt/ensenso:ro \
  ghcr.io/viamrobotics/viam-cpp-sdk:latest \
  bash -c "make build"
```

**Option B: Install Viam SDK Locally**
```bash
# Clone the Viam C++ SDK
git clone https://github.com/viamrobotics/viam-cpp-sdk.git ~/viam-cpp-sdk
cd ~/viam-cpp-sdk

# Build and install
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja all
sudo ninja install

# Return to project
cd ~/Repos/ensenso
```

### 2. Fix API Usage (TODO)

The code currently has a small issue - it references `NxLibContext` which doesn't exist in the nxLib API. The correct approach is:

```cpp
// In init_nxlib():
nxLibInitialize(true);  // Wait for cameras to enumerate

// In cleanup:
nxLibFinalize();
```

This needs to be updated in `src/module/ensenso_camera.cpp`.

### 3. Build the Module

```bash
make build
```

### 4. Test with Your Camera

```bash
# First, verify your camera is detected
/opt/ensenso/bin/nxView

# Note the serial number, then add it to a config file
# See etc/example-config.json

# Run the module
./bin/viam-camera-ensenso
```

## Code Changes Needed

1. **Remove NxLibContext** from `ensenso_camera.hpp` and `.cpp`
2. **Replace with** direct calls to `nxLibInitialize()` / `nxLibFinalize()`
3. **Add proper library lifecycle management** in the module

Would you like me to make these changes now?

## Architecture Overview

```
User's Viam Robot Config
         ↓
    Viam RDK
         ↓
Your Ensenso Module (this repo)
         ↓
    Ensenso nxLib SDK
         ↓
   Ensenso Camera (hardware)
```

## Development Workflow

1. **Edit** source files in `src/module/`
2. **Build** with `make build`
3. **Test** locally with your camera
4. **Package** with `make module.tar.gz`
5. **Deploy** to your robot via Viam configuration
