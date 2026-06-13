#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build/ci}"
INSTALL_DIR="${INSTALL_DIR:-$ROOT_DIR/build/install}"
DIST_DIR="${DIST_DIR:-$ROOT_DIR/build/dist}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
TARGET="${TARGET:-BitcoinNodeDesktop}"
APP_NAME="${APP_NAME:-Bitcoin-Qt}"
ARTIFACT_NAME="${ARTIFACT_NAME:-bitcoin-qt-$(uname -s)-$(uname -m)}"
STAGE_DIR="$DIST_DIR/$ARTIFACT_NAME"

rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

case "$(uname -s)" in
  Darwin)
    APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
    if [[ ! -d "$APP_BUNDLE" ]]; then
      APP_BUNDLE="$INSTALL_DIR/$APP_NAME.app"
    fi
    test -d "$APP_BUNDLE"

    MACDEPLOYQT="$(command -v macdeployqt || true)"
    if [[ -z "$MACDEPLOYQT" && -n "${Qt6_DIR:-}" ]]; then
      MACDEPLOYQT="$(cd "$(dirname "$Qt6_DIR")/../../bin" 2>/dev/null && pwd)/macdeployqt"
    fi
    if [[ -x "$MACDEPLOYQT" ]]; then
      "$MACDEPLOYQT" "$APP_BUNDLE" -verbose=1
    else
      echo "macdeployqt not found; packaging app bundle without Qt deployment." >&2
    fi

    cp -R "$APP_BUNDLE" "$STAGE_DIR/"
    ;;
  Linux)
    APP_BIN="$INSTALL_DIR/bin/$TARGET"
    if [[ ! -x "$APP_BIN" ]]; then
      APP_BIN="$BUILD_DIR/$TARGET"
    fi
    test -x "$APP_BIN"

    mkdir -p "$STAGE_DIR/bin"
    cp "$APP_BIN" "$STAGE_DIR/bin/$APP_NAME"
    if [[ -d "$ROOT_DIR/runtime" ]]; then
      mkdir -p "$STAGE_DIR/runtime"
      cp -a "$ROOT_DIR/runtime/." "$STAGE_DIR/runtime/"
      rm -f "$STAGE_DIR/runtime/README.md"
    fi
    ;;
  MINGW*|MSYS*|CYGWIN*)
    APP_EXE="$BUILD_DIR/$BUILD_TYPE/$TARGET.exe"
    if [[ ! -f "$APP_EXE" ]]; then
      APP_EXE="$INSTALL_DIR/bin/$TARGET.exe"
    fi
    test -f "$APP_EXE"

    mkdir -p "$STAGE_DIR"
    cp "$APP_EXE" "$STAGE_DIR/$APP_NAME.exe"

    WINDEPLOYQT="$(command -v windeployqt || true)"
    if [[ -z "$WINDEPLOYQT" && -n "${Qt6_DIR:-}" ]]; then
      WINDEPLOYQT="$(cd "$(dirname "$Qt6_DIR")/../../bin" 2>/dev/null && pwd)/windeployqt.exe"
    fi
    if [[ -x "$WINDEPLOYQT" ]]; then
      "$WINDEPLOYQT" --release --compiler-runtime "$STAGE_DIR/$APP_NAME.exe"
    else
      echo "windeployqt not found; packaging executable without Qt deployment." >&2
    fi

    if [[ -d "$ROOT_DIR/runtime" ]]; then
      mkdir -p "$STAGE_DIR/runtime"
      cp -a "$ROOT_DIR/runtime/." "$STAGE_DIR/runtime/"
      rm -f "$STAGE_DIR/runtime/README.md"
    fi
    ;;
  *)
    echo "Unsupported packaging platform: $(uname -s)" >&2
    exit 1
    ;;
esac

cp "$ROOT_DIR/README.md" "$ROOT_DIR/LICENSE" "$STAGE_DIR/"

(
  cd "$DIST_DIR"
  if [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* || "$(uname -s)" == CYGWIN* ]]; then
    7z a "$ARTIFACT_NAME.zip" "$ARTIFACT_NAME" >/dev/null
  else
    tar -czf "$ARTIFACT_NAME.tar.gz" "$ARTIFACT_NAME"
  fi
)

echo "Packaged artifact in $DIST_DIR"
