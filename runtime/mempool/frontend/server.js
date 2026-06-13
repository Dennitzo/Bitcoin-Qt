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
const requestLogFile = process.env.MEMPOOL_FRONTEND_REQUEST_LOG || '/tmp/bitcoin-qt-mempool-frontend.log';
const fallbackCacheTtlMs = 30000;

let fallbackBlocksCache = null;
let fallbackBlocksCacheAt = 0;
let fallbackBlocksInFlight = null;
let fallbackMempoolCache = null;
let fallbackMempoolCacheAt = 0;
let fallbackMempoolInFlight = null;

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

function logRequest(message) {
  fs.appendFile(requestLogFile, `${new Date().toISOString()} ${message}\n`, () => {});
}

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
  return query ? `${normalized}?${query}` : normalized;
}

function isBackendRequest(urlPath) {
  const normalized = apiPath(urlPath);
  return normalized === '/api' || normalized.startsWith('/api/');
}

function jsonHeaders() {
  return {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  };
}

function htmlHeaders() {
  return {
    'Content-Type': 'text/html; charset=utf-8',
    'Cache-Control': 'no-store',
    'Clear-Site-Data': '"cache"',
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
      timeout: 10000,
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

function fetchJson(url) {
  return new Promise((resolve, reject) => {
    const request = http.get(url, {
      timeout: 8000,
      headers: {'user-agent': 'Bitcoin-Qt IBD mempool fallback'},
    }, (response) => {
      const chunks = [];
      response.on('data', (chunk) => chunks.push(chunk));
      response.on('end', () => {
        if ((response.statusCode || 500) >= 400) {
          reject(new Error(`HTTP ${response.statusCode}`));
          return;
        }
        try {
          resolve(JSON.parse(Buffer.concat(chunks).toString('utf8')));
        } catch (error) {
          reject(error);
        }
      });
    });
    request.on('timeout', () => request.destroy(new Error('Mempool backend fallback timeout')));
    request.on('error', reject);
  });
}

function backendUrl(pathname) {
  return `http://${backendHost}:${backendPort}${pathname}`;
}

function satsPerVbyte(tx) {
  const vsize = Number(tx.vsize || tx.size || 0);
  if (!vsize) return 0;
  return Number(((Number(tx.modifiedfee ?? tx.fee ?? 0) * 100000000) / vsize).toFixed(2));
}

function percentile(sortedValues, pct) {
  if (!sortedValues.length) return 0;
  const index = Math.min(sortedValues.length - 1, Math.max(0, Math.floor((sortedValues.length - 1) * pct)));
  return Number(sortedValues[index].toFixed(2));
}

function feeRange(txs) {
  const rates = txs.map((tx) => tx.rate).filter((rate) => Number.isFinite(rate)).sort((a, b) => a - b);
  return [0, 0.1, 0.25, 0.5, 0.75, 0.9, 1].map((pct) => percentile(rates, pct));
}

function feeHistogram(txs) {
  const buckets = new Map();
  for (const tx of txs) {
    const bucket = Math.max(1, Math.floor(tx.rate));
    buckets.set(bucket, (buckets.get(bucket) || 0) + tx.vsize);
  }
  return [...buckets.entries()].sort((a, b) => b[0] - a[0]);
}

function recommendedFees(txs) {
  const rates = txs.map((tx) => tx.rate).filter((rate) => Number.isFinite(rate)).sort((a, b) => a - b);
  const minimumFee = Math.max(1, Math.ceil(percentile(rates, 0.1)));
  const economyFee = Math.max(minimumFee, Math.ceil(percentile(rates, 0.25)));
  const hourFee = Math.max(economyFee, Math.ceil(percentile(rates, 0.5)));
  const halfHourFee = Math.max(hourFee, Math.ceil(percentile(rates, 0.75)));
  const fastestFee = Math.max(halfHourFee, Math.ceil(percentile(rates, 0.9)));
  return {fastestFee, halfHourFee, hourFee, economyFee, minimumFee};
}

function buildMempoolBlocks(txs) {
  const maxBlockVsize = 1000000;
  const blocks = [];
  let current = {blockSize: 0, blockVSize: 0, nTx: 0, totalFees: 0, medianFee: 0, feeRange: [0, 0, 0, 0, 0, 0, 0], txs: []};
  for (const tx of txs) {
    if (current.nTx > 0 && current.blockVSize + tx.vsize > maxBlockVsize) {
      current.medianFee = percentile(current.txs.map((item) => item.rate).sort((a, b) => a - b), 0.5);
      current.feeRange = feeRange(current.txs);
      delete current.txs;
      blocks.push(current);
      current = {blockSize: 0, blockVSize: 0, nTx: 0, totalFees: 0, medianFee: 0, feeRange: [0, 0, 0, 0, 0, 0, 0], txs: []};
    }
    current.blockSize += tx.size;
    current.blockVSize += tx.vsize;
    current.nTx += 1;
    current.totalFees += tx.feeSats;
    current.txs.push(tx);
  }
  if (current.nTx > 0) {
    current.medianFee = percentile(current.txs.map((item) => item.rate).sort((a, b) => a - b), 0.5);
    current.feeRange = feeRange(current.txs);
    delete current.txs;
    blocks.push(current);
  }
  return blocks.slice(0, 8);
}

function transformRawMempool(info, rawMempool) {
  const txs = Object.entries(rawMempool || {}).map(([txid, tx]) => {
    const vsize = Number(tx.vsize || tx.size || 0);
    const weight = Number(tx.weight || vsize * 4);
    const feeSats = Math.round(Number(tx.modifiedfee ?? tx.fee ?? 0) * 100000000);
    return {
      txid,
      vsize,
      size: Number(tx.size || vsize),
      weight,
      feeSats,
      value: feeSats,
      rate: satsPerVbyte(tx),
      time: Number(tx.time || 0),
    };
  }).filter((tx) => tx.vsize > 0).sort((a, b) => b.rate - a.rate);

  const mempoolBlocks = buildMempoolBlocks(txs);
  const fees = recommendedFees(txs);
  return {
    sourceBackend: 'none',
    info: {
      loaded: Boolean(info.loaded ?? true),
      size: Number(info.size || txs.length),
      bytes: Number(info.bytes || txs.reduce((sum, tx) => sum + tx.vsize, 0)),
      usage: Number(info.usage || 0),
      total_fee: Number(info.total_fee || (txs.reduce((sum, tx) => sum + tx.feeSats, 0) / 100000000)),
      maxmempool: Number(info.maxmempool || 0),
      mempoolminfee: Number(info.mempoolminfee || 0),
      minrelaytxfee: Number(info.minrelaytxfee || 0),
    },
    summary: {
      count: Number(info.size || txs.length),
      vsize: Number(info.bytes || txs.reduce((sum, tx) => sum + tx.vsize, 0)),
      total_fee: Math.round(Number(info.total_fee || 0) * 100000000) || txs.reduce((sum, tx) => sum + tx.feeSats, 0),
      fee_histogram: feeHistogram(txs),
    },
    fees: {...fees, mempoolBlocks},
    mempoolBlocks,
    recent: txs.sort((a, b) => b.time - a.time).slice(0, 10).map((tx) => ({
      txid: tx.txid,
      fee: tx.feeSats,
      vsize: tx.vsize,
      value: tx.value,
      rate: tx.rate,
      time: tx.time,
    })),
  };
}

async function fallbackMempool() {
  const now = Date.now();
  if (fallbackMempoolCache && now - fallbackMempoolCacheAt < fallbackCacheTtlMs) {
    return fallbackMempoolCache;
  }
  if (fallbackMempoolInFlight) {
    return fallbackMempoolInFlight;
  }

  fallbackMempoolInFlight = Promise.all([
    rpcCall('getmempoolinfo'),
    rpcCall('getrawmempool', [true]),
  ]).then(([info, rawMempool]) => {
    const localMempool = transformRawMempool(info, rawMempool);
    fallbackMempoolCache = localMempool;
    logRequest(`fallback local mempool count=${fallbackMempoolCache.summary.count}`);
    fallbackMempoolCacheAt = Date.now();
    fallbackMempoolInFlight = null;
    return fallbackMempoolCache;
  }).catch((error) => {
    fallbackMempoolInFlight = null;
    throw error;
  });

  return fallbackMempoolInFlight;
}

function feeRangeFromBlockStats(stats) {
  const percentiles = Array.isArray(stats?.feerate_percentiles)
    ? stats.feerate_percentiles.map((value) => Number(value || 0))
    : [];
  if (!percentiles.length) {
    return [0, 0, 0, 0, 0, 0, 0];
  }
  return [
    percentiles[0],
    percentiles[0],
    percentiles[1] ?? percentiles[0],
    percentiles[2] ?? percentiles[1] ?? percentiles[0],
    percentiles[3] ?? percentiles[2] ?? percentiles[0],
    percentiles[4] ?? percentiles[3] ?? percentiles[0],
    percentiles[4] ?? percentiles[3] ?? percentiles[0],
  ];
}

function blockFromCore(block, stats = null) {
  const totalFees = Number(stats?.totalfee || 0);
  const subsidy = Number(stats?.subsidy || 0);
  const avgFeeRate = Number(stats?.avgfeerate || 0);
  const feeRange = feeRangeFromBlockStats(stats);
  const medianFee = Number(stats?.medianfeerate || feeRange[3] || avgFeeRate || 0);

  return {
    id: block.hash,
    height: block.height,
    version: block.version,
    timestamp: block.time,
    bits: parseInt(block.bits || '0', 16),
    nonce: block.nonce || 0,
    difficulty: block.difficulty || 0,
    merkle_root: block.merkleroot || '',
    tx_count: Number(stats?.txs || (Array.isArray(block.tx) ? block.tx.length : (block.nTx || 0))),
    size: Number(stats?.total_size || block.size || 0),
    weight: Number(stats?.total_weight || block.weight || 0),
    previousblockhash: block.previousblockhash || '',
    mediantime: block.mediantime || block.time,
    stale: false,
    extras: {
      reward: subsidy + totalFees,
      totalFees,
      avgFee: avgFeeRate,
      avgFeeRate,
      pool: {id: 0, name: 'Bitcoin Core', slug: 'bitcoin-core'},
      coinbaseRaw: '',
      coinbaseAddress: '',
      coinbaseSignature: '',
      coinbaseSignatureAscii: '',
      header: '',
      matchRate: 0,
      expectedFees: totalFees,
      expectedWeight: Number(stats?.total_weight || block.weight || 0),
      similarity: 0,
      cpfp: 0,
      medianFee,
      feeRange,
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

function maxBlockHeight(blocks) {
  return (Array.isArray(blocks) ? blocks : [])
    .reduce((max, block) => Math.max(max, Number(block?.height || 0)), 0);
}

function outputType(scriptPubKey = {}) {
  const type = scriptPubKey.type || '';
  if (type === 'pubkeyhash') return 'p2pkh';
  if (type === 'scripthash') return 'p2sh';
  if (type === 'witness_v0_keyhash') return 'v0_p2wpkh';
  if (type === 'witness_v0_scripthash') return 'v0_p2wsh';
  if (type === 'witness_v1_taproot') return 'v1_p2tr';
  if (type === 'nulldata') return 'op_return';
  return type || 'unknown';
}

function txFeeSats(tx) {
  if (tx.fee != null) {
    return Math.max(0, Math.round(Number(tx.fee || 0) * 100000000));
  }
  const input = Number(tx.vin?.reduce((sum, vin) => sum + Number(vin.prevout?.value || 0), 0) || 0);
  const output = Number(tx.vout?.reduce((sum, vout) => sum + Number(vout.value || 0), 0) || 0);
  return Math.max(0, Math.round(input - output));
}

function transformCoreTransaction(tx, block) {
  const status = {
    confirmed: true,
    block_height: block.height,
    block_hash: block.hash,
    block_time: block.time,
  };
  const vout = (tx.vout || []).map((output) => {
    const scriptPubKey = output.scriptPubKey || {};
    const address = Array.isArray(scriptPubKey.addresses) ? scriptPubKey.addresses[0] : scriptPubKey.address;
    return {
      scriptpubkey: scriptPubKey.hex || '',
      scriptpubkey_asm: scriptPubKey.asm || '',
      scriptpubkey_type: outputType(scriptPubKey),
      scriptpubkey_address: address,
      value: Math.round(Number(output.value || 0) * 100000000),
    };
  });
  const vin = (tx.vin || []).map((input) => ({
    txid: input.txid || '',
    vout: Number(input.vout ?? 0),
    is_coinbase: Boolean(input.coinbase),
    scriptsig: input.scriptSig?.hex || input.coinbase || '',
    scriptsig_asm: input.scriptSig?.asm || '',
    sequence: input.sequence,
    witness: input.txinwitness || [],
    prevout: {
      scriptpubkey: '',
      scriptpubkey_asm: '',
      scriptpubkey_type: 'unknown',
      value: 0,
    },
  }));
  const fee = txFeeSats(tx);
  const vsize = Number(tx.vsize || Math.ceil(Number(tx.weight || 0) / 4) || tx.size || 0);
  return {
    txid: tx.txid,
    version: tx.version,
    locktime: tx.locktime,
    size: Number(tx.size || 0),
    weight: Number(tx.weight || 0),
    fee,
    vin,
    vout,
    status,
    feePerVsize: vsize ? fee / vsize : 0,
    effectiveFeePerVsize: vsize ? fee / vsize : 0,
  };
}

function transformCoreTransactionSummary(tx) {
  const fee = txFeeSats(tx);
  const vsize = Number(tx.vsize || Math.ceil(Number(tx.weight || 0) / 4) || tx.size || 0);
  return {
    txid: tx.txid,
    fee,
    vsize,
    value: Math.round(Number(tx.vout?.reduce((sum, output) => sum + Number(output.value || 0), 0) || 0) * 100000000),
    rate: vsize ? fee / vsize : 0,
    flags: null,
    context: 'actual',
  };
}

async function loadCoreBlockWithTransactions(hash) {
  return rpcCall('getblock', [hash, 2]);
}

async function loadFallbackBlocks(info) {
  const tip = Number(info.blocks || 0);
  try {
    const backendBlocks = await fetchJson(backendUrl('/api/v1/blocks'));
    if (Array.isArray(backendBlocks) && backendBlocks.length && maxBlockHeight(backendBlocks) >= tip) {
      return sortBlocksOldestFirst(backendBlocks).slice(-8);
    }
  } catch {
    // Fall through to Core RPC. During IBD, backend init-data can be empty while
    // the blocks endpoint still has enough cached data for the first render.
  }

  const start = Math.max(0, tip - 7);
  const blocks = [];
  for (let height = start; height <= tip; height += 1) {
    const hash = await rpcCall('getblockhash', [height]);
    const [block, stats] = await Promise.all([
      rpcCall('getblockheader', [hash, true]),
      rpcCall('getblockstats', [height]).catch(() => null),
    ]);
    blocks.push(blockFromCore(block, stats));
  }
  return sortBlocksOldestFirst(blocks);
}

async function fallbackBlocks(info = null) {
  const now = Date.now();
  const blockchainInfo = info || await rpcCall('getblockchaininfo');
  const tip = Number(blockchainInfo.blocks || 0);
  if (fallbackBlocksCache && now - fallbackBlocksCacheAt < fallbackCacheTtlMs && maxBlockHeight(fallbackBlocksCache) >= tip) {
    return fallbackBlocksCache;
  }
  if (fallbackBlocksInFlight) {
    return fallbackBlocksInFlight;
  }

  fallbackBlocksInFlight = (async () => {
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

async function fallbackResponse(urlPath, method = 'GET') {
  const normalizedPath = apiPath(urlPath);
  const cleanPath = apiPath(urlPath).split('?')[0];
  if (cleanPath === '/api/v1/init-data') {
    const info = await rpcCall('getblockchaininfo');
    const blocks = await fallbackBlocks(info);
    const mempool = await fallbackMempool();
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(fallbackInitPayload(info, blocks, mempool)),
    };
  }
  if (isBlocksPath(cleanPath)) {
    if (cleanPath !== '/api/v1/blocks') {
      return null;
    }
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(await fallbackBlocks()),
    };
  }
  if (cleanPath === '/api/v1/fees/mempool-blocks') {
    const mempool = await fallbackMempool();
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(mempool.mempoolBlocks),
    };
  }
  if (cleanPath === '/api/v1/mempool') {
    const mempool = await fallbackMempool();
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(mempool.summary),
    };
  }
  if (cleanPath === '/api/v1/mempool/recent') {
    const mempool = await fallbackMempool();
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(mempool.recent),
    };
  }
  if (cleanPath === '/api/v1/fees/recommended') {
    const mempool = await fallbackMempool();
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify({
        fastestFee: mempool.fees.fastestFee,
        halfHourFee: mempool.fees.halfHourFee,
        hourFee: mempool.fees.hourFee,
        economyFee: mempool.fees.economyFee,
        minimumFee: mempool.fees.minimumFee,
      }),
    };
  }
  if (cleanPath === '/api/v1/fees/precise') {
    const mempool = await fallbackMempool();
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(mempool.fees),
    };
  }
  const blockSummaryMatch = cleanPath.match(/^\/api\/v1\/block\/([0-9a-fA-F]{64})\/summary$/);
  if (blockSummaryMatch) {
    const block = await loadCoreBlockWithTransactions(blockSummaryMatch[1]);
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify((block.tx || []).map(transformCoreTransactionSummary)),
    };
  }
  const blockTransactionsMatch = cleanPath.match(/^\/api\/block\/([0-9a-fA-F]{64})\/txs\/(\d+)$/);
  if (blockTransactionsMatch) {
    const block = await loadCoreBlockWithTransactions(blockTransactionsMatch[1]);
    const start = Number(blockTransactionsMatch[2] || 0);
    const transactions = (block.tx || [])
      .slice(start, start + 25)
      .map((tx) => transformCoreTransaction(tx, block));
    return {
      statusCode: 200,
      headers: jsonHeaders(),
      body: JSON.stringify(transactions),
    };
  }
  return null;
}

function fallbackInitPayload(info, blocks, mempool) {
  const backend = mempool.sourceBackend || 'electrum';
  return {
    backend,
    mempoolInfo: mempool.info,
    vBytesPerSecond: 0,
    blocks,
    conversions: {},
    da: {
      progressPercent: 0,
      difficultyChange: 0,
      estimatedRetargetDate: 0,
      remainingBlocks: 0,
      remainingTime: 0,
      previousRetarget: 0,
      previousTime: 0,
      nextRetargetHeight: 0,
      timeAvg: 0,
      adjustedTimeAvg: 0,
      timeOffset: 0,
      expectedBlocks: 0,
    },
    'mempool-blocks': mempool.mempoolBlocks,
    rbfSummary: [],
    stratumJobs: {},
    transactions: mempool.recent,
    backendInfo: {
      hostname: 'localhost',
      version: 'bitcoin-qt',
      gitCommit: '',
      lightning: false,
      backend,
      coreVersion: '',
      osVersion: '',
    },
    loadingIndicators: {},
    fees: mempool.fees,
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
  const [blocks, mempool] = await Promise.all([fallbackBlocks(info), fallbackMempool()]);
  const init = fallbackInitPayload(info, blocks, mempool);
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
  if (keys.includes('transactions')) {
    response.transactions = init.transactions;
  }
  return response;
}

function attachFallbackWebsocket(socket) {
  const heartbeat = setInterval(() => {
    if (socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({pong: true}));
    }
  }, 20000);
  const liveUpdates = setInterval(() => {
    if (socket.readyState !== WebSocket.OPEN) {
      return;
    }
    fallbackWebsocketPayload(['blocks', 'mempool-blocks', 'stats'])
      .then((payload) => {
        if (socket.readyState === WebSocket.OPEN) {
          socket.send(JSON.stringify(payload));
        }
      })
      .catch(() => {});
  }, 5000);
  const clearTimers = () => {
    clearInterval(heartbeat);
    clearInterval(liveUpdates);
  };
  socket.on('close', clearTimers);
  socket.on('error', clearTimers);

  fallbackWebsocketPayload(['init']).then((payload) => {
    if (socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify(payload));
    }
  }).catch(() => {});

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

function isBlocksPath(cleanPath) {
  return cleanPath === '/api/v1/blocks' || /^\/api\/v1\/blocks\/\d+$/.test(cleanPath);
}

function prefersCoreFallback(urlPath) {
  const cleanPath = apiPath(urlPath).split('?')[0];
  if (isBlocksPath(cleanPath) || isCoreBlockDataPath(cleanPath)) {
    return true;
  }
  return [
    '/api/v1/init-data',
    '/api/v1/fees/mempool-blocks',
    '/api/v1/mempool',
    '/api/v1/mempool/recent',
    '/api/v1/fees/recommended',
    '/api/v1/fees/precise',
  ].includes(cleanPath);
}

function isCoreBlockDataPath(cleanPath) {
  return /^\/api\/v1\/block\/[0-9a-fA-F]{64}\/summary$/.test(cleanPath)
    || /^\/api\/block\/[0-9a-fA-F]{64}\/txs\/\d+$/.test(cleanPath);
}

async function isInitialBlockDownload() {
  const info = await rpcCall('getblockchaininfo');
  return Boolean(info.initialblockdownload);
}

function shouldUseFallback(urlPath, statusCode, body) {
  const cleanPath = apiPath(urlPath).split('?')[0];
  const text = body.toString('utf8').trim();
  if (statusCode === 503) {
    return true;
  }
  if (cleanPath === '/api/v1/init-data') {
    if (!text || text === '{}') {
      return true;
    }
    try {
      const parsed = JSON.parse(text);
      const mempoolSize = Number(parsed?.mempoolInfo?.size || 0);
      const mempoolBlocks = Array.isArray(parsed?.['mempool-blocks']) ? parsed['mempool-blocks'].length : 0;
      if ((!parsed?.backend || parsed.backend === 'none') && mempoolSize === 0 && mempoolBlocks === 0) {
        return true;
      }
    } catch {
      return true;
    }
  }
  if (cleanPath === '/api/v1/mempool' && text) {
    try {
      const parsed = JSON.parse(text);
      if (Number(parsed?.count || 0) === 0) {
        return true;
      }
    } catch {
      return true;
    }
  }
  if ((isBlocksPath(cleanPath) || isCoreBlockDataPath(cleanPath)) && (statusCode >= 400 || !text || text === '[]')) {
    return true;
  }
  return false;
}

function proxyHttp(req, res) {
  logRequest(`http ${req.method} ${req.url} api=${isBackendRequest(req.url || '/')} normalized=${apiPath(req.url || '/')}`);
  if (prefersCoreFallback(req.url || '/')) {
    fallbackResponse(req.url || '/', req.method).then((fallback) => {
      if (!fallback) {
        proxyHttpToBackend(req, res);
        return;
      }
      logRequest(`fallback response ${apiPath(req.url || '/').split('?')[0]} bytes=${Buffer.byteLength(fallback.body)}`);
      res.writeHead(fallback.statusCode, fallback.headers);
      res.end(fallback.body);
    }).catch((error) => {
      logRequest(`fallback failed ${apiPath(req.url || '/').split('?')[0]} ${error.message}`);
      const cleanPath = apiPath(req.url || '/').split('?')[0];
      if (cleanPath === '/api/v1/blocks' && fallbackBlocksCache) {
        res.writeHead(200, jsonHeaders());
        res.end(JSON.stringify(fallbackBlocksCache));
        return;
      }
      proxyHttpToBackend(req, res);
    });
    return;
  }

  proxyHttpToBackend(req, res);
}

function proxyHttpToBackend(req, res) {
  const proxy = http.request({
    hostname: backendHost,
    port: backendPort,
    path: apiPath(req.url || '/'),
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
        fallbackResponse(req.url || '/', req.method).then((fallback) => {
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
    fallbackResponse(req.url || '/', req.method).then((fallback) => {
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
  res.writeHead(200, file.endsWith('.html') ? htmlHeaders() : {'Content-Type': contentType(file)});
  fs.createReadStream(file).pipe(res);
});

server.on('upgrade', (req, socket, head) => {
  logRequest(`upgrade ${req.url} api=${isBackendRequest(req.url || '/')} normalized=${apiPath(req.url || '/')}`);
  if (!isBackendRequest(req.url || '/')) {
    socket.destroy();
    return;
  }

  fallbackWss.handleUpgrade(req, socket, head, (ws) => {
    attachFallbackWebsocket(ws);
  });
});

function proxyWebsocket(req, socket, head) {
  const proxy = http.request({
    hostname: backendHost,
    port: backendPort,
    path: apiPath(req.url || '/'),
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
