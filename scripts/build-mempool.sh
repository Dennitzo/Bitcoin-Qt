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

rsync -a \
  --exclude '.git' \
  --exclude 'test' \
  --exclude 'tests' \
  "$BUILD_DIR/mempool/backend/" "$PREFIX/backend/"

rsync -a \
  "$BUILD_DIR/mempool/frontend/dist/" "$PREFIX/frontend/dist/"

cat > "$PREFIX/backend/server.js" <<'JS'
process.chdir(__dirname);
process.argv = [process.argv[0], require.resolve('./dist/index.js')];
require('./dist/index.js');
JS

cat > "$PREFIX/frontend/server.js" <<'JS'
const http = require('http');
const fs = require('fs');
const path = require('path');

const port = Number(process.env.MEMPOOL_FRONTEND_PORT || process.env.PORT || 8080);
const root = path.join(__dirname, 'dist');

function contentType(file) {
  if (file.endsWith('.html')) return 'text/html; charset=utf-8';
  if (file.endsWith('.js')) return 'application/javascript; charset=utf-8';
  if (file.endsWith('.css')) return 'text/css; charset=utf-8';
  if (file.endsWith('.json')) return 'application/json; charset=utf-8';
  if (file.endsWith('.svg')) return 'image/svg+xml';
  if (file.endsWith('.png')) return 'image/png';
  if (file.endsWith('.jpg') || file.endsWith('.jpeg')) return 'image/jpeg';
  if (file.endsWith('.webp')) return 'image/webp';
  if (file.endsWith('.ico')) return 'image/x-icon';
  return 'application/octet-stream';
}

function resolveFile(urlPath) {
  const cleanPath = decodeURIComponent(urlPath.split('?')[0]).replace(/^\/+/, '');
  const candidate = path.normalize(path.join(root, cleanPath));
  if (!candidate.startsWith(root)) return null;
  if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) return candidate;
  const index = path.join(root, 'index.html');
  return fs.existsSync(index) ? index : null;
}

http.createServer((req, res) => {
  const file = resolveFile(req.url || '/');
  if (!file) {
    res.writeHead(404);
    res.end('Not found');
    return;
  }
  res.writeHead(200, {'Content-Type': contentType(file)});
  fs.createReadStream(file).pipe(res);
}).listen(port, '127.0.0.1', () => {
  console.log(`Mempool frontend serving ${root} on http://127.0.0.1:${port}`);
});
JS

test -f "$PREFIX/backend/dist/index.js"
test -f "$PREFIX/frontend/server.js"
echo "Mempool staged at $PREFIX"
