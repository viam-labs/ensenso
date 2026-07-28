#!/bin/bash
# Installs the Ensenso NxLib SDK. Shared by bin/setup.sh (build time) and
# first_run.sh (runtime on the deployment target).
#
# Requires ENSENSO_SDK_URL to be set to a direct download URL for the .deb
# package matching the host distro/arch. IDS doesn't publish a stable
# versioned URL structure like Zivid does, so the deployer picks the URL
# from https://www.ensenso.com/download/ and passes it in.
#
# On Ubuntu, installs via apt so dpkg state and udev rules land correctly.
# On Debian, extracts the payload with tar (Ubuntu-only apt deps would fail
# to resolve against Debian's package set).
set -euo pipefail

if [[ -f /opt/ensenso/development/c/include/nxLib.h ]]; then
    echo "Ensenso NxLib SDK already installed."
    exit 0
fi

if [[ -z "${ENSENSO_SDK_URL:-}" ]]; then
    echo "install-ensenso-sdk.sh: ENSENSO_SDK_URL is not set" >&2
    exit 1
fi

OS="$(uname -s)"
if [[ "$OS" != "Linux" ]]; then
    echo "install-ensenso-sdk.sh: only Linux is supported (got $OS)" >&2
    exit 1
fi

if [[ ! -f /etc/os-release ]]; then
    echo "install-ensenso-sdk.sh: cannot detect distribution (missing /etc/os-release)" >&2
    exit 1
fi
# shellcheck disable=SC1091
. /etc/os-release

case "${ID:-},${VERSION_ID:-}" in
    ubuntu,20.04|ubuntu,22.04|ubuntu,24.04) INSTALL_MODE="apt" ;;
    debian,11|debian,12)                    INSTALL_MODE="extract" ;;
    *)
        echo "install-ensenso-sdk.sh: unsupported distribution ${ID:-unknown}/${VERSION_ID:-unknown}" >&2
        exit 1
        ;;
esac

ARCH="$(dpkg --print-architecture)"
case "$ARCH" in
    amd64) ARCH_MARKER="x64" ;;
    arm64) ARCH_MARKER="arm64" ;;
    *)
        echo "install-ensenso-sdk.sh: unsupported architecture $ARCH" >&2
        exit 1
        ;;
esac

if [[ "$ENSENSO_SDK_URL" != *"$ARCH_MARKER"* ]]; then
    echo "install-ensenso-sdk.sh: ENSENSO_SDK_URL doesn't match host architecture $ARCH (expected '$ARCH_MARKER' in URL): $ENSENSO_SDK_URL" >&2
    exit 1
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
ENSENSO_DEB="${WORKDIR}/ensenso-sdk.deb"

echo "Downloading Ensenso SDK from ${ENSENSO_SDK_URL}..."
curl --fail --silent --show-error --location "${ENSENSO_SDK_URL}" -o "${ENSENSO_DEB}"

case "$INSTALL_MODE" in
    apt)
        echo "Installing package via apt..."
        sudo apt-get update
        sudo apt-get install -y --no-install-recommends "${ENSENSO_DEB}"
        ;;
    extract)
        echo "Extracting package contents (Ubuntu .deb on Debian, no dpkg state)..."
        sudo apt-get install -y --no-install-recommends zstd
        UNPACK_DIR="${WORKDIR}/unpack"
        mkdir -p "${UNPACK_DIR}"
        ( cd "${UNPACK_DIR}" && ar x "${ENSENSO_DEB}" )
        DATA_TAR=$(ls "${UNPACK_DIR}"/data.tar.* | head -n1)
        sudo tar -axf "${DATA_TAR}" -C /
        sudo ldconfig
        ;;
esac

if [[ ! -f /opt/ensenso/development/c/include/nxLib.h ]]; then
    echo "install-ensenso-sdk.sh: install completed but nxLib.h not found at expected path" >&2
    exit 1
fi

echo "Ensenso NxLib SDK install complete."
