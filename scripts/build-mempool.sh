#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/runtime-src"
PREFIX="$ROOT_DIR/runtime/mempool"
NODE="$ROOT_DIR/runtime/node/bin/node"
NPM="$ROOT_DIR/runtime/node/bin/npm"
REF="${MEMPOOL_REF:-master}"

if [[ ! -x "$NODE" || ! -x "$NPM" ]]; then
  "$ROOT_DIR/scripts/build-node-runtime.sh"
fi

mkdir -p "$BUILD_DIR" "$PREFIX"

if [[ ! -d "$BUILD_DIR/mempool/.git" ]]; then
  git clone https://github.com/mempool/mempool.git "$BUILD_DIR/mempool"
fi

cd "$BUILD_DIR/mempool"
git fetch --tags --prune
git checkout "$REF"

for package_dir in backend frontend; do
  cd "$BUILD_DIR/mempool/$package_dir"
  if [[ -f package-lock.json ]]; then
    "$NPM" ci
  else
    "$NPM" install
  fi
  "$NPM" run build
done

rm -rf "$PREFIX"
mkdir -p "$PREFIX/backend" "$PREFIX/frontend"

cp -a "$BUILD_DIR/mempool/backend/." "$PREFIX/backend/"
rm -rf "$PREFIX/backend/.git" "$PREFIX/backend/test" "$PREFIX/backend/tests"

mkdir -p "$PREFIX/frontend/dist"
cp -a "$BUILD_DIR/mempool/frontend/dist/." "$PREFIX/frontend/dist/"

cat > "$PREFIX/backend/server.js" <<'JS'
process.chdir(__dirname);
process.argv = [process.argv[0], require.resolve('./dist/index.js')];
require('./dist/index.js');
JS

cp "$ROOT_DIR/scripts/mempool-frontend-server.js" "$PREFIX/frontend/server.js"
chmod +x "$PREFIX/frontend/server.js"

test -f "$PREFIX/backend/dist/index.js"
test -f "$PREFIX/frontend/server.js"
echo "Mempool staged at $PREFIX"
