#!/bin/bash
set -euxo pipefail

OS=$(uname -s | tr '[:upper:]' '[:lower:]')

if [[ ${OS} == "linux" ]]; then
    sudo apt-get update
    sudo apt-get install -y wget gpg lsb-release

    # Add Kitware repository for up-to-date CMake on Ubuntu
    if lsb_release -is | grep -q "Ubuntu"; then
        sudo wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | sudo gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
        sudo echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
        sudo apt-get update
    fi

    sudo apt-get install -y \
        python3 \
        python3-venv \
        python3-pip \
        cmake \
        cmake-data \
        autoconf \
        automake \
        build-essential \
        ca-certificates \
        curl \
        g++ \
        git \
        gnupg \
        libssl-dev \
        ninja-build \
        pkg-config \
        software-properties-common
elif [[ ${OS} == "darwin" ]]; then
    if ! command -v brew >/dev/null 2>&1; then
        echo "Homebrew not found. Please install it first: https://brew.sh/"
        exit 1
    fi
    brew install cmake pkg-config ninja python
fi

# Check python3 availability
if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 not found in PATH. Aborting." >&2
    exit 1
fi

# Check venv module
if ! python3 -m venv --help >/dev/null 2>&1; then
    echo "python3 venv module not available. Try: sudo apt-get install --reinstall python3-venv python3-full python3-pip" >&2
    exit 1
fi

# Create venv if it doesn't exist
if [ ! -f "./venv/bin/activate" ]; then
    echo 'creating virtual env'
    python3 -m venv venv
fi

source ./venv/bin/activate

# Install conan into venv if not present
if [ ! -f "./venv/bin/conan" ]; then
    echo 'installing conan'
    pip install --upgrade pip
    pip install conan
fi

conan profile detect || echo "Conan profile already exists"
conan remote add viamconan https://viam.jfrog.io/artifactory/api/conan/viamconan --index 0 || echo "Viam conan remote already exists"
