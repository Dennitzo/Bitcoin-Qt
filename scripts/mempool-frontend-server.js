const http = require('http');
const fs = require('fs');
const path = require('path');

const port = Number(process.env.MEMPOOL_FRONTEND_PORT || process.env.PORT || 8080);
const backendPort = Number(process.env.MEMPOOL_BACKEND_PORT || 8999);
const backendHost = process.env.MEMPOOL_BACKEND_HOST || '127.0.0.1';

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
  if (path.extname(cleanPath)) return null;
  const index = path.join(root, 'index.html');
  return fs.existsSync(index) ? index : null;
}

function apiPath(urlPath) {
  const [pathname, query = ''] = urlPath.split('?');
  const normalized = pathname.replace(/^\/[a-z]{2}(?:-[A-Z]{2})?\/api(?=\/|$)/, '/api');
  const prefixed = normalized === '/api'
    ? '/api/v1'
    : normalized.startsWith('/api/v1/')
      ? normalized
      : normalized.startsWith('/api/')
        ? `/api/v1${normalized.slice('/api'.length)}`
        : normalized;
  return query ? `${prefixed}?${query}` : prefixed;
}

function isBackendRequest(urlPath) {
  const normalized = apiPath(urlPath);
  return normalized === '/api' || normalized.startsWith('/api/');
}

function proxyHttp(req, res) {
  const proxy = http.request({
    hostname: backendHost,
    port: backendPort,
    path: apiPath(req.url || '/'),
    method: req.method,
    headers: {...req.headers, host: `${backendHost}:${backendPort}`},
  }, (backendRes) => {
    res.writeHead(backendRes.statusCode || 502, {...backendRes.headers, 'Cache-Control': 'no-store'});
    backendRes.pipe(res);
  });
  proxy.setTimeout(30000, () => proxy.destroy(new Error('Mempool backend timeout')));
  proxy.on('error', (error) => {
    res.writeHead(502, {'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store'});
    res.end(JSON.stringify({error: `Mempool backend unavailable: ${error.message}`}));
  });
  req.pipe(proxy);
}

const server = http.createServer((req, res) => {
  if ((req.url || '/') === '/bitcoin-qt-health') {
    res.writeHead(200, {'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store'});
    res.end(JSON.stringify({ok: true, backendPort}));
    return;
  }
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
  res.writeHead(200, {'Content-Type': contentType(file), 'Cache-Control': file.endsWith('.html') ? 'no-store' : 'public, max-age=3600'});
  fs.createReadStream(file).pipe(res);
});

server.on('upgrade', (req, socket, head) => {
  if (!isBackendRequest(req.url || '/')) {
    socket.destroy();
    return;
  }
  const proxy = http.request({
    hostname: backendHost,
    port: backendPort,
    path: apiPath(req.url || '/'),
    method: req.method,
    headers: {...req.headers, host: `${backendHost}:${backendPort}`},
  });
  proxy.on('upgrade', (backendRes, backendSocket, backendHead) => {
    const headers = Object.entries(backendRes.headers)
      .filter(([key]) => !['connection', 'upgrade'].includes(key.toLowerCase()))
      .map(([key, value]) => `${key}: ${value}`);
    socket.write([
      'HTTP/1.1 101 Switching Protocols',
      'Upgrade: websocket',
      'Connection: Upgrade',
      ...headers,
      '',
      '',
    ].join('\r\n'));
    if (backendHead.length) socket.write(backendHead);
    if (head.length) backendSocket.write(head);
    backendSocket.pipe(socket);
    socket.pipe(backendSocket);
  });
  proxy.on('error', () => socket.destroy());
  proxy.end();
});

server.listen(port, '127.0.0.1', () => {
  console.log(`Mempool frontend serving ${root} on http://127.0.0.1:${port}, proxying /api to http://${backendHost}:${backendPort}`);
});
