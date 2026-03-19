OUTPUT_NAME := viam-camera-ensenso
CONAN_OUTPUT := build-conan
CMAKE_BUILD_DIR := $(CONAN_OUTPUT)/build/Release
BIN_DIR := bin
BINARY := viam-camera-ensenso

export CONAN_FLAGS := -s:a build_type=Release -s:a compiler.cppstd=17

.PHONY: setup build conan-build test-sdk check-sdk clean

default: module.tar.gz

# Verify Ensenso SDK is installed
check-sdk:
	@echo "Checking Ensenso SDK at /opt/ensenso..."
	@if [ ! -f /opt/ensenso/development/c/include/nxLib.h ]; then \
		echo "ERROR: Ensenso SDK not found at /opt/ensenso"; \
		echo "  Install from: https://www.ids-imaging.com/ensenso-sdk.html"; \
		exit 1; \
	fi
	@echo "  nxLib.h found"
	@if [ ! -f /opt/ensenso/lib/libNxLib64.so ]; then \
		echo "ERROR: libNxLib64.so not found in /opt/ensenso/lib"; \
		exit 1; \
	fi
	@echo "  libNxLib64.so found"
	@echo "Ensenso SDK OK"

# Install system dependencies, create venv, install conan (run once)
setup: check-sdk
	bin/setup.sh

# Install Viam SDK via Conan and compile
build:
	test -f ./venv/bin/activate && . ./venv/bin/activate; \
	conan install . \
		--output-folder=$(CONAN_OUTPUT) \
		--build=missing \
		$(CONAN_FLAGS)
	test -f ./venv/bin/activate && . ./venv/bin/activate; \
	cmake --preset conan-release
	test -f ./venv/bin/activate && . ./venv/bin/activate; \
	cmake --build $(CMAKE_BUILD_DIR) --config Release
	@mkdir -p $(BIN_DIR)
	@cp $(CMAKE_BUILD_DIR)/$(BINARY) $(BIN_DIR)/$(BINARY)
	@echo "Binary: $(BIN_DIR)/$(BINARY)"

# One-step: setup + build
conan-build: setup build

# Create deployable package
module.tar.gz: build
	@echo "Creating module.tar.gz..."
	tar czf module.tar.gz \
		-C $(BIN_DIR) $(BINARY) \
		-C $(shell pwd)/etc meta.json
	@echo "Created module.tar.gz"

# Build and run the Ensenso SDK integration test
test-sdk:
	test -f ./venv/bin/activate && . ./venv/bin/activate; \
	conan install . \
		--output-folder=$(CONAN_OUTPUT) \
		--build=missing \
		$(CONAN_FLAGS)
	test -f ./venv/bin/activate && . ./venv/bin/activate; \
	cmake --preset conan-release
	test -f ./venv/bin/activate && . ./venv/bin/activate; \
	cmake --build $(CMAKE_BUILD_DIR) --target test-ensenso-sdk --config Release
	$(CMAKE_BUILD_DIR)/test-ensenso-sdk

# Remove all build artifacts
clean:
	rm -rf $(CONAN_OUTPUT) $(BIN_DIR) module.tar.gz venv
	@echo "Clean complete"
