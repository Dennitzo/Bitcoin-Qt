#!/usr/bin/env bash
set -euo pipefail


ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOWNLOAD_DIR="$ROOT_DIR/build/runtime-downloads"
PREFIX="$ROOT_DIR/runtime/node"
NODE_VERSION="${NODE_VERSION:-v24.13.0}"


case "$(uname -s):$(uname -m)" in
    Darwin:arm64) NODE_ARCH="darwin-arm64"; ARCHIVE_EXT="tar.xz" ;;
    Darwin:x86_64) NODE_ARCH="darwin-x64"; ARCHIVE_EXT="tar.xz" ;;
    Linux:x86_64) NODE_ARCH="linux-x64"; ARCHIVE_EXT="tar.xz" ;;
    MINGW*:x86_64|MSYS*:x86_64|CYGWIN*:x86_64) NODE_ARCH="win-x64"; ARCHIVE_EXT="zip" ;;
    *) echo "Unsupported Node runtime platform: $(uname -s) $(uname -m)" >&2; exit 1 ;;
esac


ARCHIVE="node-${NODE_VERSION}-${NODE_ARCH}.${ARCHIVE_EXT}"
URL="https://nodejs.org/dist/${NODE_VERSION}/${ARCHIVE}"


mkdir -p "$DOWNLOAD_DIR" "$PREFIX"
if [[ ! -f "$DOWNLOAD_DIR/$ARCHIVE" ]]; then
    curl -L "$URL" -o "$DOWNLOAD_DIR/$ARCHIVE"
fi


rm -rf "$PREFIX"
mkdir -p "$PREFIX"
case "$ARCHIVE_EXT" in
    tar.xz)
        tar -xJf "$DOWNLOAD_DIR/$ARCHIVE" -C "$PREFIX" --strip-components=1
        ;;
    zip)
        TMP_DIR="$DOWNLOAD_DIR/node-${NODE_VERSION}-${NODE_ARCH}"
        rm -rf "$TMP_DIR"
        mkdir -p "$TMP_DIR"
        7z x "$DOWNLOAD_DIR/$ARCHIVE" "-o$TMP_DIR" >/dev/null
        cp -a "$TMP_DIR/node-${NODE_VERSION}-${NODE_ARCH}/." "$PREFIX/"
        mkdir -p "$PREFIX/bin"
        cp "$PREFIX/node.exe" "$PREFIX/bin/node.exe"
        cat > "$PREFIX/bin/node" <<'SH'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$DIR/node.exe" "$@"
SH
cat > "$PREFIX/bin/npm" <<'SH'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$DIR/npm.cmd" "$@"
SH
chmod +x "$PREFIX/bin/node" "$PREFIX/bin/npm"
        ;;
esac

test -x "$PREFIX/bin/node" || test -x "$PREFIX/bin/node.exe"

"$PREFIX/bin/node" --version
