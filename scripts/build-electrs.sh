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

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    if [[ -z "${CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER:-}" ]]; then
      for candidate in \
        "/c/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC"/*/bin/Hostx64/x64/link.exe \
        "/c/Program Files/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64/link.exe \
        "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC"/*/bin/Hostx64/x64/link.exe; do
        if [[ -x "$candidate" ]]; then
          export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER="$candidate"
          break
        fi
      done
    fi
    if [[ -z "${CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER:-}" ]]; then
      echo "MSVC link.exe not found. Ensure the Visual Studio C++ build tools are available." >&2
      exit 1
    fi
    ;;
esac

if [[ ! -d "$BUILD_DIR/electrs/.git" ]]; then
  git clone https://github.com/romanz/electrs.git "$BUILD_DIR/electrs"
fi

cd "$BUILD_DIR/electrs"
git fetch --tags --prune
git checkout "$REF"
cargo build --release --locked

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) ELECTRS_BIN="electrs.exe" ;;
  *) ELECTRS_BIN="electrs" ;;
esac

cp "target/release/$ELECTRS_BIN" "$PREFIX/bin/$ELECTRS_BIN"
chmod +x "$PREFIX/bin/$ELECTRS_BIN"
"$PREFIX/bin/$ELECTRS_BIN" --version
