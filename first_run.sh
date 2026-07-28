#!/bin/bash
# Installs the Ensenso NxLib SDK needed by viam-camera-ensenso.
set -euo pipefail

if [[ -f /opt/ensenso/development/c/include/nxLib.h ]]; then
    echo "Ensenso NxLib SDK already installed."
    exit 0
fi

if [[ -z "${ENSENSO_SDK_URL:-}" ]]; then
    cat >&2 <<EOF
first_run.sh: Ensenso NxLib SDK not installed and no download URL provided.

Set the following environment variable before running this script (find
the current .deb download URL for your distro/arch at
https://www.ensenso.com/download/):

  ENSENSO_SDK_URL=<full url to the ensenso .deb>

Or install the Ensenso SDK manually to /opt/ensenso before running the module.
EOF
    exit 1
fi

export ENSENSO_SDK_URL
exec "$(dirname "$0")/install-ensenso-sdk.sh"
