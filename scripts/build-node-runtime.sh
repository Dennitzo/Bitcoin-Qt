#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOWNLOAD_DIR="$ROOT_DIR/build/runtime-downloads"
PREFIX="$ROOT_DIR/runtime/node"
NODE_VERSION="${NODE_VERSION:-v24.13.0}"

case "$(uname -m)" in
  arm64) NODE_ARCH="darwin-arm64" ;;
  x86_64) NODE_ARCH="darwin-x64" ;;
  *) echo "Unsupported macOS architecture: $(uname -m)" >&2; exit 1 ;;
esac

ARCHIVE="node-${NODE_VERSION}-${NODE_ARCH}.tar.xz"
URL="https://nodejs.org/dist/${NODE_VERSION}/${ARCHIVE}"

mkdir -p "$DOWNLOAD_DIR" "$PREFIX"
if [[ ! -f "$DOWNLOAD_DIR/$ARCHIVE" ]]; then
  curl -L "$URL" -o "$DOWNLOAD_DIR/$ARCHIVE"
fi

rm -rf "$PREFIX"
mkdir -p "$PREFIX"
tar -xJf "$DOWNLOAD_DIR/$ARCHIVE" -C "$PREFIX" --strip-components=1

test -x "$PREFIX/bin/node"
"$PREFIX/bin/node" --version
