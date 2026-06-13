#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/ci}"
INSTALL_DIR="${INSTALL_DIR:-$ROOT_DIR/build/install}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-}"

cmake_args=(
  -S "$ROOT_DIR"
  -B "$BUILD_DIR"
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"
)

if [[ -n "${CMAKE_PREFIX_PATH:-}" ]]; then
  cmake_args+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH")
elif [[ -n "${Qt6_DIR:-}" ]]; then
  cmake_args+=("-DQt6_DIR=$Qt6_DIR")
fi

if command -v ninja >/dev/null 2>&1; then
  cmake_args+=("-G" "Ninja")
fi

cmake "${cmake_args[@]}"

build_args=(--build "$BUILD_DIR" --config "$BUILD_TYPE")
if [[ -n "$JOBS" ]]; then
  build_args+=(--parallel "$JOBS")
fi
cmake "${build_args[@]}"

cmake --install "$BUILD_DIR" --config "$BUILD_TYPE"
