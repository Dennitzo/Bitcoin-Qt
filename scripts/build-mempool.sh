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
const WebSocket = require(path.join(__dirname, '..', 'backend', 'node_modules', 'ws'));

const port = Number(process.env.MEMPOOL_FRONTEND_PORT || process.env.PORT || 8080);
const backendPort = Number(process.env.MEMPOOL_BACKEND_PORT || 8999);
const backendHost = process.env.MEMPOOL_BACKEND_HOST || '127.0.0.1';
const bitcoinRpcPort = Number(process.env.MEMPOOL_BITCOIN_RPC_PORT || 8345);
const bitcoinRpcUser = process.env.MEMPOOL_BITCOIN_RPC_USER || 'bitcoin';
const bitcoinRpcPassword = process.env.MEMPOOL_BITCOIN_RPC_PASSWORD || 'bitcoin';
const fallbackCacheTtlMs = 30000;

let fallbackBlocksCache = null;
let fallbackBlocksCacheAt = 0;
let fallbackBlocksInFlight = null;

function firstExisting(paths) {
  for (const candidate of paths) {
    if (fs.existsSync(path.join(candidate, 'index.html'))) {
      return candidate;
    }
  }
  return paths[0];
}

const root = firstExisting([
  path.join(__dirname, 'dist', 'mempool', 'browser', 'de'),
  path.join(__dirname, 'dist', 'mempool', 'browser', 'en-US'),
  path.join(__dirname, 'dist'),
]);
const browserRoot = path.join(__dirname, 'dist', 'mempool', 'browser');
const fallbackWss = new WebSocket.Server({noServer: true});

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
  let cleanPath = decodeURIComponent(urlPath.split('?')[0]).replace(/^\/+/, '');
  cleanPath = cleanPath.replace(/^(de|en-US)\//, '');
  const base = cleanPath.startsWith('resources/') ? browserRoot : root;
  const candidate = path.normalize(path.join(base, cleanPath));
  if (!candidate.startsWith(base)) return null;
  if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) return candidate;
  const index = path.join(root, 'index.html');
  return fs.existsSync(index) ? index : null;
}

function isBackendRequest(urlPath) {
  return urlPath === '/api' || urlPath.startsWith('/api/');
}

function jsonHeaders() {
  return {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  };
}

function rpcCall(method, params = []) {
  const body = JSON.stringify({jsonrpc: '1.0', id: 'bitcoin-qt-mempool-fallback', method, params});
  const auth = Buffer.from(`${bitcoinRpcUser}:${bitcoinRpcPassword}`).toString('base64');
  return new Promise((resolve, reject) => {
    const request = http.request({
      hostname: '127.0.0.1',
      port: bitcoinRpcPort,
      path: '/',
      method: 'POST',
      timeout: 2500,
      headers: {
        authorization: `Basic ${auth}`,
        'content-type': 'text/plain',
        'content-length': Buffer.byteLength(body),
      },
    }, (response) => {
      const chunks = [];
      response.on('data', (chunk) => chunks.push(chunk));
      response.on('end', () => {
        try {
          const payload = JSON.parse(Buffer.concat(chunks).toString('utf8'));
          if (payload.error) {
            reject(new Error(payload.error.message || 'Bitcoin RPC error'));
            return;
          }
          resolve(payload.result);
        } catch (error) {
          reject(error);
        }
      });
    });
    request.on('timeout', () => request.destroy(new Error('Bitcoin RPC timeout')));
    request.on('error', reject);
    request.end(body);
  });
}

function blockFromCore(block) {
  return {
    id: block.hash,
    height: block.height,
    version: block.version,
    timestamp: block.time,
    bits: parseInt(block.bits || '0', 16),
    nonce: block.nonce || 0,
    difficulty: block.difficulty || 0,
    merkle_root: block.merkleroot || '',
    tx_count: Array.isArray(block.tx) ? block.tx.length : (block.nTx || 0),
    size: block.size || 0,
    weight: block.weight || 0,
    previousblockhash: block.previousblockhash || '',
    mediantime: block.mediantime || block.time,
    stale: false,
    extras: {
      reward: 0,
      totalFees: 0,
      avgFee: 0,
      avgFeeRate: 0,
      pool: {id: 0, name: 'Bitcoin Core', slug: 'bitcoin-core'},
      coinbaseRaw: '',
      coinbaseAddress: '',
      coinbaseSignature: '',
      coinbaseSignatureAscii: '',
      header: '',
      matchRate: 0,
      expectedFees: 0,
      expectedWeight: 0,
      similarity: 0,
      cpfp: 0,
      medianFee: 0,
      feeRange: [0, 0, 0, 0, 0, 0, 0],
      totalInputs: 0,
      totalOutputs: 0,
      segwitTotalTxs: 0,
      segwitTotalSize: 0,
      segwitTotalWeight: 0,
      firstSeen: null,
      utxoSetChange: 0,
      utxoSetSize: null,
      totalInputAmt: null,
    },
  };
}

function sortBlocksOldestFirst(blocks) {
  return (Array.isArray(blocks) ? blocks : [])
    .filter(Boolean)
    .sort((a, b) => Number(a.height || 0) - Number(b.height || 0));
}

async function loadFallbackBlocks(info) {
  const tip = Number(info.blocks || 0);
  const start = Math.max(0, tip - 7);
  const blocks = [];
  for (let height = start; height <= tip; height += 1) {
    const hash = await rpcCall('getblockhash', [height]);
    const block = await rpcCall('getblock', [hash, 1]);
    blocks.push(blockFromCore(block));
  }
  return sortBlocksOldestFirst(blocks);
}

async function fallbackBlocks(info = null) {
  const now = Date.now();
  if (fallbackBlocksCache && now - fallbackBlocksCacheAt < fallbackCacheTtlMs) {
    return fallbackBlocksCache;
  }
  if (fallbackBlocksInFlight) {
    return fallbackBlocksInFlight;
  }

  fallbackBlocksInFlight = (async () => {
    const blockchainInfo = info || await rpcCall('getblockchaininfo');
    const blocks = await loadFallbackBlocks(blockchainInfo);
    fallbackBlocksCache = sortBlocksOldestFirst(blocks);
    fallbackBlocksCacheAt = Date.now();
    fallbackBlocksInFlight = null;
    return fallbackBlocksCache;
  })().catch((error) => {
    fallbackBlocksInFlight = null;
    throw error;
  });

  return fallbackBlocksInFlight;
}

async function fallbackResponse(urlPath) {
  const cleanPath = urlPath.split('?')[0];
  if (cleanPath === '/api/v1/init-data') {
    const info = await rpcCall('getblockchaininfo');
    const blocks = await fallbackBlocks(info);
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(fallbackInitPayload(info, blocks)),
    };
  }
  if (cleanPath === '/api/v1/blocks') {
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(await fallbackBlocks()),
    };
  }
  if (cleanPath === '/api/v1/fees/mempool-blocks') {
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify([]),
    };
  }
  if (cleanPath === '/api/v1/mempool') {
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify({count: 0, vsize: 0, total_fee: 0, fee_histogram: []}),
    };
  }
  if (cleanPath === '/api/v1/fees/recommended') {
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify({
        fastestFee: 1,
        halfHourFee: 1,
        hourFee: 1,
        economyFee: 1,
        minimumFee: 1,
      }),
    };
  }
  if (cleanPath === '/api/v1/fees/precise') {
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify({
        fastestFee: 1,
        halfHourFee: 1,
        hourFee: 1,
        economyFee: 1,
        minimumFee: 1,
        mempoolBlocks: [],
      }),
    };
  }
  return null;
}

function fallbackInitPayload(info, blocks) {
  return {
    backend: 'bitcoin-core-ibd',
    mempoolInfo: {loaded: true, size: 0, bytes: 0, usage: 0, total_fee: 0},
    vBytesPerSecond: 0,
    blocks,
    conversions: {},
    'mempool-blocks': [],
    transactions: [],
    backendInfo: {
      hostname: 'localhost',
      version: 'bitcoin-qt',
      gitCommit: '',
      lightning: false,
      backend: 'bitcoin-core-ibd',
      coreVersion: '',
      osVersion: '',
    },
    loadingIndicators: {},
    fees: {
      fastestFee: 1,
      halfHourFee: 1,
      hourFee: 1,
      economyFee: 1,
      minimumFee: 1,
      mempoolBlocks: [],
    },
    bitcoinCoreInfo: {
      blocks: info.blocks,
      headers: info.headers,
      initialblockdownload: info.initialblockdownload,
      verificationprogress: info.verificationprogress,
    },
  };
}

async function fallbackWebsocketPayload(keys = []) {
  const info = await rpcCall('getblockchaininfo');
  const blocks = await fallbackBlocks(info);
  const init = fallbackInitPayload(info, blocks);
  if (!keys.length || keys.includes('init')) {
    return init;
  }
  const response = {};
  if (keys.includes('blocks')) {
    response.blocks = init.blocks;
  }
  if (keys.includes('mempool-blocks')) {
    response['mempool-blocks'] = init['mempool-blocks'];
  }
  if (keys.includes('stats')) {
    response.mempoolInfo = init.mempoolInfo;
    response.vBytesPerSecond = init.vBytesPerSecond;
    response.fees = init.fees;
  }
  return response;
}

function attachFallbackWebsocket(socket) {
  socket.on('message', (raw) => {
    let message = {};
    try {
      message = JSON.parse(raw.toString('utf8'));
    } catch {
      socket.close();
      return;
    }

    if (message.action === 'ping') {
      socket.send(JSON.stringify({pong: true}));
      return;
    }

    const wants = [];
    if (message.action === 'init') {
      wants.push('init');
    }
    if (message.action === 'want' && Array.isArray(message.data)) {
      wants.push(...message.data);
    }
    if (message['refresh-blocks']) {
      wants.push('blocks');
    }

    fallbackWebsocketPayload(wants).then((payload) => {
      if (socket.readyState === WebSocket.OPEN) {
        socket.send(JSON.stringify(payload));
      }
    }).catch(() => {
      if (socket.readyState === WebSocket.OPEN) {
        socket.close();
      }
    });
  });
}

function prefersCoreFallback(urlPath) {
  const cleanPath = urlPath.split('?')[0];
  return [
    '/api/v1/init-data',
    '/api/v1/blocks',
    '/api/v1/fees/mempool-blocks',
    '/api/v1/mempool',
    '/api/v1/fees/recommended',
    '/api/v1/fees/precise',
  ].includes(cleanPath);
}

async function isInitialBlockDownload() {
  const info = await rpcCall('getblockchaininfo');
  return Boolean(info.initialblockdownload);
}

function shouldUseFallback(urlPath, statusCode, body) {
  const cleanPath = urlPath.split('?')[0];
  const text = body.toString('utf8').trim();
  if (statusCode === 503) {
    return true;
  }
  if (cleanPath === '/api/v1/init-data' && (!text || text === '{}')) {
    return true;
  }
  if (cleanPath === '/api/v1/blocks' && (!text || text === '[]')) {
    return true;
  }
  return false;
}

function proxyHttp(req, res) {
  if (prefersCoreFallback(req.url || '/')) {
    isInitialBlockDownload().then((isIbd) => {
      if (!isIbd) {
        proxyHttpToBackend(req, res);
        return;
      }
      return fallbackResponse(req.url || '/').then((fallback) => {
        if (!fallback) {
          proxyHttpToBackend(req, res);
          return;
        }
        res.writeHead(fallback.statusCode, fallback.headers);
        res.end(fallback.body);
      });
    }).catch(() => proxyHttpToBackend(req, res));
    return;
  }

  proxyHttpToBackend(req, res);
}

function proxyHttpToBackend(req, res) {
  const proxy = http.request({
    hostname: backendHost,
    port: backendPort,
    path: req.url,
    method: req.method,
    headers: {
      ...req.headers,
      host: `${backendHost}:${backendPort}`,
    },
  }, (backendRes) => {
    const chunks = [];
    backendRes.on('data', (chunk) => chunks.push(chunk));
    backendRes.on('end', () => {
      const body = Buffer.concat(chunks);
      if (shouldUseFallback(req.url || '/', backendRes.statusCode || 502, body)) {
        fallbackResponse(req.url || '/').then((fallback) => {
          if (fallback) {
            res.writeHead(fallback.statusCode, fallback.headers);
            res.end(fallback.body);
            return;
          }
          res.writeHead(backendRes.statusCode || 502, backendRes.headers);
          res.end(body);
        }).catch(() => {
          res.writeHead(backendRes.statusCode || 502, backendRes.headers);
          res.end(body);
        });
        return;
      }
      res.writeHead(backendRes.statusCode || 502, {
        ...backendRes.headers,
        'Cache-Control': 'no-store',
      });
      res.end(body);
    });
  });

  proxy.setTimeout(4000, () => proxy.destroy(new Error('Mempool backend timeout')));
  proxy.on('error', (error) => {
    fallbackResponse(req.url || '/').then((fallback) => {
      if (fallback) {
        res.writeHead(fallback.statusCode, fallback.headers);
        res.end(fallback.body);
        return;
      }
      res.writeHead(502, jsonHeaders());
      res.end(JSON.stringify({error: `Mempool backend unavailable: ${error.message}`}));
    }).catch(() => {
      res.writeHead(502, jsonHeaders());
      res.end(JSON.stringify({error: `Mempool backend unavailable: ${error.message}`}));
    });
  });

  req.pipe(proxy);
}

const server = http.createServer((req, res) => {
  if (isBackendRequest(req.url || '/')) {
    proxyHttp(req, res);
    return;
  }

  const file = resolveFile(req.url || '/');
  if (!file) {
    res.writeHead(404);
    res.end('Not found');
    return;
  }
  res.writeHead(200, {'Content-Type': contentType(file)});
  fs.createReadStream(file).pipe(res);
});

server.on('upgrade', (req, socket, head) => {
  if (!isBackendRequest(req.url || '/')) {
    socket.destroy();
    return;
  }

  isInitialBlockDownload().then((isIbd) => {
    if (!isIbd) {
      proxyWebsocket(req, socket, head);
      return;
    }

    fallbackWss.handleUpgrade(req, socket, head, (ws) => {
      attachFallbackWebsocket(ws);
    });
  }).catch(() => proxyWebsocket(req, socket, head));
});

function proxyWebsocket(req, socket, head) {
  const proxy = http.request({
    hostname: backendHost,
    port: backendPort,
    path: req.url,
    method: req.method,
    headers: {
      ...req.headers,
      host: `${backendHost}:${backendPort}`,
    },
  });

  proxy.on('upgrade', (backendRes, backendSocket, backendHead) => {
    socket.write([
      'HTTP/1.1 101 Switching Protocols',
      'Upgrade: websocket',
      'Connection: Upgrade',
      ...Object.entries(backendRes.headers).map(([key, value]) => `${key}: ${value}`),
      '',
      '',
    ].join('\r\n'));
    if (backendHead.length) {
      socket.write(backendHead);
    }
    if (head.length) {
      backendSocket.write(head);
    }
    backendSocket.pipe(socket);
    socket.pipe(backendSocket);
  });

  proxy.on('error', () => socket.destroy());
  proxy.end();
}

server.listen(port, '127.0.0.1', () => {
  console.log(`Mempool frontend serving ${root} on http://127.0.0.1:${port}, proxying /api to http://${backendHost}:${backendPort}`);
});
JS

test -f "$PREFIX/backend/dist/index.js"
test -f "$PREFIX/frontend/server.js"
echo "Mempool staged at $PREFIX"
