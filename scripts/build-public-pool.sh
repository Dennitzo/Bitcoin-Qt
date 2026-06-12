#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/runtime-src"
BACKEND_SRC="$BUILD_DIR/public-pool"
FRONTEND_SRC="$BUILD_DIR/public-pool-ui"
RUNTIME_DIR="$ROOT_DIR/runtime/public-pool"
NODE_BIN="$ROOT_DIR/runtime/node/bin/node"
NPM_BIN="$ROOT_DIR/runtime/node/bin/npm"

mkdir -p "$BUILD_DIR" "$RUNTIME_DIR/backend" "$RUNTIME_DIR/frontend"

if [[ ! -x "$NODE_BIN" || ! -x "$NPM_BIN" ]]; then
  echo "Node runtime missing. Run scripts/build-node-runtime.sh first." >&2
  exit 1
fi

if [[ ! -d "$BACKEND_SRC/.git" ]]; then
  git clone https://github.com/benjamin-wilson/public-pool.git "$BACKEND_SRC"
else
  git -C "$BACKEND_SRC" pull --ff-only --autostash
fi

if [[ ! -d "$FRONTEND_SRC/.git" ]]; then
  git clone https://github.com/benjamin-wilson/public-pool-ui.git "$FRONTEND_SRC"
else
  git -C "$FRONTEND_SRC" pull --ff-only --autostash
fi

(
  cd "$BACKEND_SRC"
  "$NPM_BIN" ci
  "$NPM_BIN" run build
)

(
  cd "$FRONTEND_SRC"
  if ! grep -q "SECURE_STRATUM_URL" src/environments/environment.electron.ts; then
    perl -0pi -e "s/STRATUM_URL: 'localhost:3333'/STRATUM_URL: 'localhost:3333',\\n    SECURE_STRATUM_URL: 'localhost:4333'/" src/environments/environment.electron.ts
  fi
  "$NPM_BIN" ci
  "$NPM_BIN" run build:electron
)

rm -rf "$RUNTIME_DIR/backend" "$RUNTIME_DIR/frontend"
mkdir -p "$RUNTIME_DIR/backend" "$RUNTIME_DIR/frontend"

rsync -a \
  --exclude '.git' \
  --exclude 'coverage' \
  --exclude 'test' \
  "$BACKEND_SRC/" "$RUNTIME_DIR/backend/"

cp -R "$FRONTEND_SRC/dist/public-pool-ui" "$RUNTIME_DIR/frontend/dist"
mkdir -p "$RUNTIME_DIR/frontend/dist/chunks" "$RUNTIME_DIR/frontend/dist/vendor/@kurkle/color"
cp "$FRONTEND_SRC/node_modules/chart.js/dist/chunks/helpers.segment.js" "$RUNTIME_DIR/frontend/dist/chunks/helpers.segment.js"
cp "$FRONTEND_SRC/node_modules/@kurkle/color/dist/color.esm.js" "$RUNTIME_DIR/frontend/dist/vendor/@kurkle/color/color.esm.js"

cat > "$RUNTIME_DIR/frontend/server.js" <<'JS'
const http = require('http');
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, 'dist');
const port = Number(process.env.PUBLIC_POOL_FRONTEND_PORT || process.env.PORT || 3335);
const apiPort = Number(process.env.PUBLIC_POOL_API_PORT || 3334);
const stratumPort = Number(process.env.PUBLIC_POOL_STRATUM_PORT || 3333);

const types = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.woff2': 'font/woff2',
};

function sendFile(res, file) {
  fs.readFile(file, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    if (path.basename(file) === 'index.html') {
      const importMap = '<script type="importmap">{"imports":{"@kurkle/color":"./vendor/@kurkle/color/color.esm.js"}}</script>';
      data = Buffer.from(data.toString('utf8')
        .replace('</head>', `${importMap}</head>`)
        .replace(/<script src="(scripts\.[^"]+\.js)" defer><\/script>/, '<script src="$1" type="module"></script>'));
    }
    res.writeHead(200, {'content-type': types[path.extname(file)] || 'application/octet-stream'});
    res.end(data);
  });
}

http.createServer((req, res) => {
  if (req.url === '/bitcoin-qt-health') {
    res.writeHead(200, {'content-type': 'application/json'});
    res.end(JSON.stringify({ok: true, apiPort}));
    return;
  }
  if (req.url === '/assets/bitcoin-qt-config.js' || req.url === '/bitcoin-qt-config.js') {
    res.writeHead(200, {'content-type': 'application/javascript; charset=utf-8'});
    res.end(`window.__PUBLIC_POOL_CONFIG__ = { API_URL: 'http://127.0.0.1:${apiPort}', STRATUM_URL: '127.0.0.1:${stratumPort}', SECURE_STRATUM_URL: '127.0.0.1:4333' };`);
    return;
  }
  const clean = decodeURIComponent((req.url || '/').split('?')[0]).replace(/^\/+/, '');
  const candidate = path.normalize(path.join(root, clean || 'index.html'));
  if (!candidate.startsWith(root)) {
    res.writeHead(403);
    res.end('Forbidden');
    return;
  }
  fs.stat(candidate, (err, stat) => {
    if (!err && stat.isFile()) {
      sendFile(res, candidate);
      return;
    }
    sendFile(res, path.join(root, 'index.html'));
  });
}).listen(port, '127.0.0.1', () => {
  console.log(`Public Pool UI listening on http://127.0.0.1:${port}`);
});
JS

echo "Public Pool runtime staged at $RUNTIME_DIR"
