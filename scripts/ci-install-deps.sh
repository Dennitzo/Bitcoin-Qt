#!/usr/bin/env bash
set -euo pipefail

case "$(uname -s)" in
  Linux)
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      ninja-build \
      patchelf \
      libcups2-dev \
      libegl1 \
      libgl1 \
      libxkbcommon-x11-0 \
      libxcb-cursor0 \
      libxcb-icccm4 \
      libxcb-image0 \
      libxcb-keysyms1 \
      libxcb-randr0 \
      libxcb-render-util0 \
      libxcb-shape0 \
      libxcb-xinerama0 \
      libxcb-xfixes0 \
      vulkan-headers
    ;;
  Darwin)
    if ! command -v cmake >/dev/null 2>&1; then
      brew install cmake
    fi
    if ! command -v ninja >/dev/null 2>&1; then
      brew install ninja
    fi
    ;;
  MINGW*|MSYS*|CYGWIN*)
    choco install cmake ninja -y --no-progress
    ;;
  *)
    echo "Unsupported CI platform: $(uname -s)" >&2
    exit 1
    ;;
esac
