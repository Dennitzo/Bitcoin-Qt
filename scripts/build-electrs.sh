#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/runtime-src"
PREFIX="$ROOT_DIR/runtime/electrs"
REF="${ELECTRS_REF:-master}"

mkdir -p "$BUILD_DIR" "$PREFIX/bin"

if [[ -f "$HOME/.cargo/env" ]]; then
  # shellcheck disable=SC1091
  source "$HOME/.cargo/env"
fi

if ! command -v cargo >/dev/null 2>&1; then
  echo "cargo/rustc not found. Install Rust first: https://rustup.rs/" >&2
  exit 1
fi

if [[ -z "${LIBCLANG_PATH:-}" ]]; then
  for candidate in \
    "/c/Program Files/LLVM/bin" \
    "/c/Program Files/LLVM/lib" \
    "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib" \
    "/Applications/Xcode.app/Contents/Frameworks" \
    "/Library/Developer/CommandLineTools/usr/lib" \
    "/usr/local/opt/llvm/lib"; do
    if [[ -f "$candidate/libclang.dylib" || -f "$candidate/libclang.dll" || -f "$candidate/clang.dll" || -f "$candidate/libclang.lib" ]]; then
      export LIBCLANG_PATH="$candidate"
      break
    fi
  done
fi

if [[ -n "${LIBCLANG_PATH:-}" ]]; then
  export DYLD_LIBRARY_PATH="$LIBCLANG_PATH${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
  export PATH="$LIBCLANG_PATH:$PATH"
fi

if [[ ! -d "$BUILD_DIR/electrs/.git" ]]; then
  git clone https://github.com/romanz/electrs.git "$BUILD_DIR/electrs"
fi

cd "$BUILD_DIR/electrs"
git fetch --tags --prune
git checkout "$REF"
cargo build --release --locked

cp target/release/electrs "$PREFIX/bin/electrs"
chmod +x "$PREFIX/bin/electrs"
"$PREFIX/bin/electrs" --version
