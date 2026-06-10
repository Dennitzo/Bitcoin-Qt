#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"$ROOT_DIR/scripts/build-bitcoin-core.sh"
"$ROOT_DIR/scripts/build-electrs.sh"
"$ROOT_DIR/scripts/build-node-runtime.sh"
"$ROOT_DIR/scripts/build-mempool.sh"

echo
echo "Runtime ready:"
find "$ROOT_DIR/runtime" -maxdepth 3 -type f \( -perm -111 -o -name 'server.js' -o -name 'package.json' \) | sort
