# Quick Reference - Viam Ensenso Camera Module

## Build Commands (In Order)

```bash
make setup          # Downloads Viam SDK 0.31.0 via Conan (run once)
make build          # Compiles the module
make module.tar.gz  # Creates deployable package
```

## One-Line Build
```bash
make setup && make module.tar.gz
```

## Testing
```bash
make test-sdk       # Test Ensenso SDK integration (needs camera)
make check-sdk      # Verify Ensenso SDK is installed
```

## Cleaning
```bash
make clean          # Remove all build artifacts
```

## Files Generated

- `build/Release/viam-camera-ensenso` - Main executable (74MB)
- `build/Release/test-ensenso-sdk` - SDK test utility
- `bin/viam-camera-ensenso` - Copy of executable
- `module.tar.gz` - Deployable package

## Robot Configuration

Use `etc/example-config.json` as a template:

```json
{
  "name": "ensenso",
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

## Project Structure

```
ensenso/
├── src/module/           # Main implementation
│   ├── ensenso_camera.hpp
│   ├── ensenso_camera.cpp
│   └── main.cpp
├── src/test/             # Tests
├── etc/                  # Configuration files
├── CMakeLists.txt        # Build configuration
├── conanfile.py          # Conan recipe (SDK 0.31.0)
└── Makefile              # Build automation
```

## Key Technical Details

- **Viam SDK**: 0.31.0 (same as RealSense module)
- **Ensenso SDK**: 4.3.905 (at /opt/ensenso)
- **Language**: C++17
- **Build System**: CMake 3.25+ with Conan
- **Binary Size**: ~74MB (includes dependencies)

## Troubleshooting

**Problem**: `make setup` fails
**Solution**: Check Conan installation: `conan --version`

**Problem**: Can't find Ensenso SDK
**Solution**: Run `make check-sdk` to verify installation

**Problem**: Build fails
**Solution**: Clean and rebuild: `make clean && make setup && make build`

## Documentation

- **BUILD.md** - Detailed build instructions
- **INSTALL.md** - Installation and deployment
- **STATUS.md** - Project status
- **SUMMARY.md** - Complete project summary

## Deployment

1. Upload `module.tar.gz` to your Viam robot
2. Add module configuration to robot config
3. Configure camera component (see example-config.json)
4. Start using the camera!

## Support

- GitHub Issues: [Your repo]/issues
- Viam Docs: https://docs.viam.com/
- Ensenso SDK: /opt/ensenso/manual/
