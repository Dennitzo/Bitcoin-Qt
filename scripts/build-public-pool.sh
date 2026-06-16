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

BACKEND_SRC="$BACKEND_SRC" "$NODE_BIN" <<'JS'
const fs = require('fs');
const path = require('path');

const backend = process.env.BACKEND_SRC;

const addressSettingsService = path.join(backend, 'src/ORM/address-settings/address-settings.service.ts');
let addressSettingsSource = fs.readFileSync(addressSettingsService, 'utf8');
if (!addressSettingsSource.includes('public async getAddresses()')) {
  addressSettingsSource = addressSettingsSource.replace(
`    public async createNew(address: string) {
        return await this.addressSettingsRepository.save({ address });
    }`,
`    public async getAddresses() {
        return await this.addressSettingsRepository.createQueryBuilder()
            .select('"address", "updatedAt", "shares", "bestDifficulty", "bestDifficultyUserAgent"')
            .orderBy('"updatedAt"', 'DESC')
            .execute();
    }

    public async createNew(address: string) {
        return await this.addressSettingsRepository.save({ address });
    }`);
  fs.writeFileSync(addressSettingsService, addressSettingsSource);
}

const appController = path.join(backend, 'src/app.controller.ts');
let appControllerSource = fs.readFileSync(appController, 'utf8');
if (!appControllerSource.includes("@Get('info/addresses')")) {
  appControllerSource = appControllerSource.replace(
`\n}\n`,
`
  @Get('info/addresses')
  public async infoAddresses() {
    return await this.addressSettingsService.getAddresses();
  }

}
`);
  fs.writeFileSync(appController, appControllerSource);
}
JS

FRONTEND_SRC="$FRONTEND_SRC" "$NODE_BIN" <<'JS'
const fs = require('fs');
const path = require('path');

const frontend = process.env.FRONTEND_SRC;

const appService = path.join(frontend, 'src/app/services/app.service.ts');
let appServiceSource = fs.readFileSync(appService, 'utf8');
if (!appServiceSource.includes('getAddresses()')) {
  appServiceSource = appServiceSource.replace(
`    public getAccounting() {
        return this.httpClient.get(\`\${this.appConfig.apiUrl}/api/info/accounting\`) as Observable<any>;
    }
}`,
`    public getAccounting() {
        return this.httpClient.get(\`\${this.appConfig.apiUrl}/api/info/accounting\`) as Observable<any>;
    }
    public getAddresses() {
        return this.httpClient.get(\`\${this.appConfig.apiUrl}/api/info/addresses\`) as Observable<any[]>;
    }
}`);
  fs.writeFileSync(appService, appServiceSource);
}

const splashComponent = path.join(frontend, 'src/app/components/splash/splash.component.ts');
let splashComponentSource = fs.readFileSync(splashComponent, 'utf8');
if (!splashComponentSource.includes('public addresses$: Observable<any[]>;')) {
  splashComponentSource = splashComponentSource
    .replace('  public networkInfo$: Observable<any>;\n', '  public networkInfo$: Observable<any>;\n  public addresses$: Observable<any[]>;\n')
    .replace(
`    this.networkInfo$ = this.appService.getNetworkInfo().pipe(
      shareReplay({ refCount: true, bufferSize: 1 })
    );
`,
`    this.networkInfo$ = this.appService.getNetworkInfo().pipe(
      shareReplay({ refCount: true, bufferSize: 1 })
    );
    this.addresses$ = this.appService.getAddresses().pipe(
      shareReplay({ refCount: true, bufferSize: 1 })
    );
`);
  fs.writeFileSync(splashComponent, splashComponentSource);
}

const splashTemplate = path.join(frontend, 'src/app/components/splash/splash.component.html');
let splashTemplateSource = fs.readFileSync(splashTemplate, 'utf8');
if (!splashTemplateSource.includes('class="card address-card"')) {
  splashTemplateSource = splashTemplateSource.replace(
`        <div class="col-12" *ngIf="accounting$ | async as accounting">`,
`        <div class="col-12">
            <div class="card address-card">
                <div class="address-card-header">
                    <div>
                        <h4 class="m-0">Pool Addresses</h4>
                        <span class="text-500">Addresses that have mined on this pool.</span>
                    </div>
                </div>

                <ng-container *ngIf="addresses$ | async as addresses; else loadingAddresses">
                    <div class="address-list" *ngIf="addresses.length > 0; else emptyAddresses">
                        <a class="address-row" *ngFor="let entry of addresses" [routerLink]="['app', entry.address]">
                            <code>{{entry.address}}</code>
                            <span>{{(entry.shares || 0) | numberSuffix}} shares</span>
                            <span>Best {{(entry.bestDifficulty || 0) | numberSuffix}}</span>
                        </a>
                    </div>
                    <ng-template #emptyAddresses>
                        <div class="address-empty">No addresses have mined on this pool yet.</div>
                    </ng-template>
                </ng-container>
                <ng-template #loadingAddresses>
                    <p-skeleton width="100%" height="4rem"></p-skeleton>
                </ng-template>
            </div>
        </div>

        <div class="col-12" *ngIf="accounting$ | async as accounting">`);
  fs.writeFileSync(splashTemplate, splashTemplateSource);
}

const splashStyles = path.join(frontend, 'src/app/components/splash/splash.component.scss');
let splashStylesSource = fs.readFileSync(splashStyles, 'utf8');
if (!splashStylesSource.includes('.address-card')) {
  splashStylesSource = splashStylesSource.replace(
`.round-card {
    padding: 1.5rem;
}`,
`.address-list {
    border: 1px solid var(--surface-border);
    overflow: hidden;
    margin-top: 1rem;
}

.address-row {
    align-items: center;
    color: var(--text-color);
    display: grid;
    gap: 1rem;
    grid-template-columns: minmax(0, 1fr) auto auto;
    padding: 0.85rem 1rem;
    text-decoration: none;
}

.address-row+.address-row {
    border-top: 1px solid var(--surface-border);
}

.address-row code {
    overflow-wrap: anywhere;
}

.address-row span {
    color: var(--text-color-secondary);
    white-space: nowrap;
}

.round-card {
    padding: 1.5rem;
}`);
  splashStylesSource = splashStylesSource.replace(
`    .hero-footer {
        flex-direction: column;
        gap: 0.25rem;
    }`,
`    .address-row {
        grid-template-columns: 1fr;
    }

    .hero-footer {
        flex-direction: column;
        gap: 0.25rem;
    }`);
  fs.writeFileSync(splashStyles, splashStylesSource);
}
JS

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

cp -a "$BACKEND_SRC/." "$RUNTIME_DIR/backend/"
rm -rf "$RUNTIME_DIR/backend/.git" "$RUNTIME_DIR/backend/coverage" "$RUNTIME_DIR/backend/test"
mkdir -p "$RUNTIME_DIR/backend/dist/api"
cat > "$RUNTIME_DIR/backend/dist/api/database-migration.js" <<'JS'
// Compatibility stub for older asset patch steps.
// Current Public Pool builds use TypeORM synchronize and no longer ship this file.
JS

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
  if (req.url === '/assets/runtime-config.js' || req.url === '/runtime-config.js' ||
      req.url === '/assets/bitcoin-qt-config.js' || req.url === '/bitcoin-qt-config.js') {
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
    if (path.extname(candidate)) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    sendFile(res, path.join(root, 'index.html'));
  });
}).listen(port, '127.0.0.1', () => {
  console.log(`Public Pool UI listening on http://127.0.0.1:${port}`);
});
JS

echo "Public Pool runtime staged at $RUNTIME_DIR"
