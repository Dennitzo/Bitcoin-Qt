#!/usr/bin/env bash
set -euo pipefail

case "$(uname -s)" in
  Linux)
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends \
      build-essential \
      autoconf \
      automake \
      clang \
      cmake \
      cargo \
      curl \
      git \
      libclang-dev \
      libtool \
      ninja-build \
      patchelf \
      pkg-config \
      rustc \
      xz-utils \
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
      libvulkan-dev
    ;;
  Darwin)
    if ! command -v cmake >/dev/null 2>&1; then
      brew install cmake
    fi
    if ! command -v ninja >/dev/null 2>&1; then
      brew install ninja
    fi
    PYTHON_VENV="$HOME/.bitcoinqt-ci-python"
    python3 -m venv "$PYTHON_VENV"
    "$PYTHON_VENV/bin/python" -m pip install --upgrade pip setuptools
    if [[ -n "${GITHUB_PATH:-}" ]]; then
      echo "$PYTHON_VENV/bin" >> "$GITHUB_PATH"
    else
      export PATH="$PYTHON_VENV/bin:$PATH"
    fi
    ;;
  MINGW*|MSYS*|CYGWIN*)
    choco install make pkgconfiglite rust -y --no-progress
    ;;
  *)
    echo "Unsupported CI platform: $(uname -s)" >&2
    exit 1
    ;;
esac
