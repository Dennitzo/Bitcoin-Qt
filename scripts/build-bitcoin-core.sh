#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/runtime-src"
PREFIX="$ROOT_DIR/runtime/bitcoin"
REF="${BITCOIN_CORE_REF:-v24.2}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
HOST="${BITCOIN_CORE_HOST:-$(clang -dumpmachine)}"

mkdir -p "$BUILD_DIR" "$PREFIX"

if [[ ! -d "$BUILD_DIR/bitcoin/.git" ]]; then
  git clone https://github.com/bitcoin/bitcoin.git "$BUILD_DIR/bitcoin"
fi

cd "$BUILD_DIR/bitcoin"
git fetch --tags --prune
git checkout "$REF"

if command -v cmake >/dev/null 2>&1; then
  CMAKE_BIN="$(command -v cmake)"
elif [[ -x "/Applications/CMake.app/Contents/bin/cmake" ]]; then
  CMAKE_BIN="/Applications/CMake.app/Contents/bin/cmake"
  export PATH="/Applications/CMake.app/Contents/bin:$PATH"
else
  echo "Missing build tool: cmake" >&2
  exit 1
fi

export LANG=C
export LC_ALL=C
export LC_CTYPE=C

for tool in pkg-config make; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Missing build tool: $tool" >&2
    echo "Install Bitcoin Core build dependencies before running this script." >&2
    exit 1
  fi
done

make -C depends -j"$JOBS" HOST="$HOST" NO_QT=1 NO_WALLET=1 NO_SQLITE=1 NO_ZMQ=1 NO_UPNP=1 NO_NATPMP=1 NO_USDT=1

if [[ -f CMakeLists.txt ]]; then
  rm -rf build-node
  "$CMAKE_BIN" -S . -B build-node \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_TOOLCHAIN_FILE="depends/$HOST/toolchain.cmake" \
    -DBUILD_DAEMON=ON \
    -DBUILD_CLI=ON \
    -DBUILD_GUI=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_BENCH=OFF \
    -DBUILD_TX=OFF \
    -DBUILD_UTIL=OFF \
    -DENABLE_WALLET=OFF \
    -DWITH_ZMQ=OFF \
    -DWITH_USDT=OFF

  "$CMAKE_BIN" --build build-node --target bitcoind bitcoin-cli -j "$JOBS"
  "$CMAKE_BIN" --install build-node --component Runtime || "$CMAKE_BIN" --install build-node
else
  ./autogen.sh
  CONFIG_SITE="$PWD/depends/$HOST/share/config.site" ./configure \
    --prefix="$PREFIX" \
    --without-gui \
    --disable-tests \
    --disable-bench \
    --disable-fuzz-binary \
    --disable-wallet \
    --disable-zmq \
    --disable-man \
    --without-miniupnpc \
    --without-natpmp

  make -j "$JOBS" src/bitcoind src/bitcoin-cli
  mkdir -p "$PREFIX/bin"
  install -m 0755 src/bitcoind "$PREFIX/bin/bitcoind"
  install -m 0755 src/bitcoin-cli "$PREFIX/bin/bitcoin-cli"
fi

test -x "$PREFIX/bin/bitcoind"
"$PREFIX/bin/bitcoind" --version | head -1
