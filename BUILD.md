# Build Instructions

This document provides detailed build instructions for the Viam Ensenso Camera Module.

## Prerequisites

### Required
- **Ensenso SDK 4.3+** installed at `/opt/ensenso/`
- **CMake 3.20+**
- **C++17 compatible compiler** (GCC 9+, Clang 10+)
- **Python 3** (for Conan)

### Verify Ensenso SDK
```bash
make check-sdk
```

## Build Methods

### Method 1: Conan Build (Recommended) ✨

Conan automatically downloads and manages the Viam C++ SDK dependency, making this the easiest approach.

#### 1. Install Conan

```bash
# Using pip
pip install conan

# Or using pipx (recommended for isolated installation)
pipx install conan

# Verify installation
conan --version
```

#### 2. Build the Module

```bash
# One command builds everything
make conan-build
```

This will:
- Download Viam C++ SDK via Conan
- Configure CMake with proper toolchain
- Build the module
- Copy executable to `bin/viam-camera-ensenso`

#### 3. Create Package

```bash
tar czf module.tar.gz \
  bin/viam-camera-ensenso \
  etc/meta.json \
  README.md
```

### Method 2: Manual Build

If you prefer to manage dependencies manually:

#### 1. Install Viam C++ SDK

```bash
git clone https://github.com/viamrobotics/viam-cpp-sdk.git
cd viam-cpp-sdk
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja all
sudo ninja install
```

#### 2. Build Module

```bash
cd /path/to/ensenso
make setup
make build
```

### Method 3: Docker Build

Build in a containerized environment with all dependencies:

```bash
# Create Dockerfile
cat > Dockerfile <<'EOF'
FROM ghcr.io/viamrobotics/viam-cpp-sdk:latest

# Copy Ensenso SDK (must be available on host)
COPY --from=host /opt/ensenso /opt/ensenso

# Set working directory
WORKDIR /workspace

# Build command
CMD ["make", "build"]
EOF

# Build
docker build -t ensenso-builder .
docker run --rm -v $(pwd):/workspace ensenso-builder
```

## Testing

### Test Ensenso SDK Integration

```bash
# Build and run SDK test
make test-sdk
```

This verifies:
- ✓ Ensenso SDK is accessible
- ✓ Library can be loaded
- ✓ Cameras can be enumerated

### Test Full Module

```bash
# Run with a physical camera connected
./bin/viam-camera-ensenso
```

## Troubleshooting

### Conan Issues

**Problem**: `conan: command not found`
```bash
# Install Conan
pip install --user conan
# Add to PATH
export PATH="$HOME/.local/bin:$PATH"
```

**Problem**: Viam SDK download fails
```bash
# Try with explicit Conan remote
conan remote add viam https://artifactory.viam.com/artifactory/api/conan/conan
conan install . --build=missing
```

### CMake Issues

**Problem**: `Could not find viam-cpp-sdk`
```bash
# Use Conan build instead
make conan-build

# Or set CMAKE_PREFIX_PATH manually
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/viam-cpp-sdk/install
```

**Problem**: `Ensenso SDK not found`
```bash
# Verify installation
ls -l /opt/ensenso/lib/libNxLib64.so

# If installed elsewhere, set path
cmake -B build -DENSENSO_SDK_DIR=/path/to/ensenso
```

### Runtime Issues

**Problem**: `libNxLib64.so: cannot open shared object file`
```bash
# Add to library path temporarily
export LD_LIBRARY_PATH=/opt/ensenso/lib:$LD_LIBRARY_PATH

# Or add permanently
echo 'export LD_LIBRARY_PATH=/opt/ensenso/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc

# Or create system-wide config
sudo sh -c 'echo "/opt/ensenso/lib" > /etc/ld.so.conf.d/ensenso.conf'
sudo ldconfig
```

**Problem**: `No cameras found`
- Ensure camera is connected via USB or GigE
- Check USB permissions: `ls -l /dev/bus/usb/`
- Add user to plugdev group: `sudo usermod -a -G plugdev $USER`
- For GigE cameras, verify network connection
- Test with nxView: `/opt/ensenso/bin/nxView`

## Build Outputs

After successful build:

```
bin/
├── viam-camera-ensenso    # Main module executable
└── test-ensenso-sdk       # SDK integration test

module.tar.gz              # Distributable package
```

## Next Steps

- See [INSTALL.md](INSTALL.md) for deployment instructions
- See [README.md](README.md) for configuration options
- See [QUICKSTART.md](QUICKSTART.md) for getting started
