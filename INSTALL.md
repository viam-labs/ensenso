# Installation Guide

This guide walks you through setting up and building the Viam Ensenso Camera module.

## Prerequisites

### 1. Ensenso SDK

The Ensenso SDK is already installed on your system at `/opt/ensenso/`.

Verify installation:
```bash
make check-sdk
```

### 2. Viam C++ SDK

Install the Viam C++ SDK:

**Option A: Using Docker (Recommended for development)**
```bash
# Pull the Viam C++ SDK Docker image
docker pull ghcr.io/viamrobotics/viam-cpp-sdk:latest

# Build using Docker
docker run --rm -v $(pwd):/workspace -w /workspace \
  ghcr.io/viamrobotics/viam-cpp-sdk:latest \
  make build
```

**Option B: Install locally**
```bash
# Clone and build Viam C++ SDK
git clone https://github.com/viamrobotics/viam-cpp-sdk.git
cd viam-cpp-sdk
mkdir build && cd build
cmake .. -G Ninja
ninja all
sudo ninja install
```

### 3. Build Tools

Install required build tools:
```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  git
```

## Building the Module

1. **Setup and build:**
   ```bash
   make setup
   make build
   ```

2. **Verify the build:**
   ```bash
   ls -lh bin/viam-camera-ensenso
   ```

3. **Create distributable package:**
   ```bash
   make module.tar.gz
   ```

## Installation

### Local Installation

Install the module to your system:
```bash
sudo make install
```

This installs:
- Binary: `/usr/local/bin/viam-camera-ensenso`
- Metadata: `/usr/local/etc/meta.json`

### Module Configuration

Add the module to your Viam robot configuration:

```json
{
  "modules": [
    {
      "type": "local",
      "name": "viam-camera-ensenso",
      "executable_path": "/usr/local/bin/viam-camera-ensenso"
    }
  ],
  "components": [
    {
      "name": "my-ensenso-camera",
      "namespace": "rdk",
      "type": "camera",
      "model": "viam:camera:ensenso",
      "attributes": {
        "serial_number": "",
        "width_px": 1280,
        "height_px": 1024,
        "enable_depth": true,
        "enable_point_cloud": true
      }
    }
  ]
}
```

## Configuration Options

| Attribute | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `serial_number` | string | No | "" | Camera serial number. If empty, uses first available camera. |
| `width_px` | int | No | 1280 | Image width in pixels |
| `height_px` | int | No | 1024 | Image height in pixels |
| `enable_depth` | bool | No | true | Enable depth map generation |
| `enable_point_cloud` | bool | No | true | Enable 3D point cloud generation |

## Testing

### Test with a physical camera

1. Connect your Ensenso camera
2. List available cameras:
   ```bash
   /opt/ensenso/bin/nxView
   ```
   or check via command line:
   ```bash
   # Use an Ensenso SDK example to list cameras
   /opt/ensenso/development/examples/C++/nxListCams/nxListCams
   ```

3. Note the serial number and add it to your config

### Test the module

Run the module directly (for debugging):
```bash
./bin/viam-camera-ensenso --help
```

## Troubleshooting

### Library not found error

If you get an error about `libNxLib64.so` not being found:

```bash
# Add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/opt/ensenso/lib:$LD_LIBRARY_PATH

# Or add permanently to your shell profile
echo 'export LD_LIBRARY_PATH=/opt/ensenso/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
```

### CMake can't find Viam SDK

Make sure you've installed the Viam C++ SDK and it's in a standard location, or set the CMake prefix path:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/viam-cpp-sdk/install
```

### Camera not found

- Ensure the camera is properly connected (USB or GigE)
- Check permissions: You may need to add your user to the `plugdev` group
- Verify with nxView: `/opt/ensenso/bin/nxView`

## Next Steps

- See [README.md](README.md) for usage information
- Check out the Viam documentation: https://docs.viam.com/
- Browse Ensenso SDK examples: `/opt/ensenso/development/examples/`
