# Viam Ensenso Camera Module

C++ module implementing the `rdk:component:camera` API for IDS Ensenso stereo cameras.

## Overview

This module allows you to integrate Ensenso 3D cameras into your Viam-managed robots, providing:
- Color/grayscale image capture
- Depth map generation
- 3D point cloud data
- Camera calibration support

## Requirements

- Ensenso SDK 4.3+ installed (`/opt/ensenso/`)
- Viam C++ SDK
- CMake 3.20+
- C++17 compatible compiler

## Building

### Option 1: Conan Build (Recommended)

Conan automatically manages the Viam C++ SDK dependency:

```bash
# Install Conan if not already installed
pip install conan

# Build the module (Conan handles Viam SDK)
make conan-build
```

### Option 2: Manual Build

Requires Viam C++ SDK to be pre-installed:

```bash
make setup      # Initialize build environment
make build      # Build the module
make module.tar.gz  # Create distributable package
make test       # Run tests
make clean      # Clean build artifacts
```

## Configuration

Add this module to your robot configuration:

```json
{
  "name": "ensenso",
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
```

## Supported Cameras

- Ensenso N series (N10, N20, N30, N35, N40, N45)
- Ensenso S series
- Ensenso X series
- Ensenso XR series

## License

See LICENSE file for details.
