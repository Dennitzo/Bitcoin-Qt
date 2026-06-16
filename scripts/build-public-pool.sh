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

function replaceOrThrow(source, pattern, replacement, label) {
  const next = source.replace(pattern, replacement);
  if (next === source) {
    throw new Error(`Public Pool patch failed: ${label}`);
  }
  return next;
}

const addressSettingsService = path.join(backend, 'src/ORM/address-settings/address-settings.service.ts');
let addressSettingsSource = fs.readFileSync(addressSettingsService, 'utf8');
if (addressSettingsSource.includes('public async getAddresses()') && !addressSettingsSource.includes('"rejectedShares"')) {
  addressSettingsSource = addressSettingsSource.replace(
`    public async getAddresses() {
        return await this.addressSettingsRepository.createQueryBuilder()
            .select('"address", "updatedAt", "shares", "bestDifficulty", "bestDifficultyUserAgent"')
            .orderBy('"updatedAt"', 'DESC')
            .execute();
    }`,
`    public async getAddresses() {
        return await this.addressSettingsRepository.createQueryBuilder()
            .select('"address", "updatedAt", "shares", "rejectedShares", "bestDifficulty", "bestDifficultyUserAgent"')
            .orderBy('"updatedAt"', 'DESC')
            .execute();
    }`);
}
if (!addressSettingsSource.includes('public async getAddresses()')) {
  addressSettingsSource = addressSettingsSource.replace(
`    public async createNew(address: string) {
        return await this.addressSettingsRepository.save({ address });
    }`,
`    public async getAddresses() {
        return await this.addressSettingsRepository.createQueryBuilder()
            .select('"address", "updatedAt", "shares", "rejectedShares", "bestDifficulty", "bestDifficultyUserAgent"')
            .orderBy('"updatedAt"', 'DESC')
            .execute();
    }

    public async createNew(address: string) {
        return await this.addressSettingsRepository.save({ address });
    }`);
}
if (!addressSettingsSource.includes('public async addRejectedShare(address: string)')) {
  addressSettingsSource = replaceOrThrow(
    addressSettingsSource,
    /\n\s+public async resetBestDifficultyAndShares\(\) \{/,
`
    public async addRejectedShare(address: string) {
        await this.addressSettingsRepository
            .createQueryBuilder()
            .insert()
            .into(AddressSettingsEntity)
            .values({ address })
            .orIgnore()
            .execute();

        return await this.addressSettingsRepository.createQueryBuilder()
            .update(AddressSettingsEntity)
            .set({
                rejectedShares: () => '"rejectedShares" + 1'
            })
            .where('address = :address', { address })
            .execute();
    }

    public async resetBestDifficultyAndShares() {`,
    'insert AddressSettingsService.addRejectedShare');
}
fs.writeFileSync(addressSettingsService, addressSettingsSource);

const addressSettingsEntity = path.join(backend, 'src/ORM/address-settings/address-settings.entity.ts');
let addressSettingsEntitySource = fs.readFileSync(addressSettingsEntity, 'utf8');
if (!addressSettingsEntitySource.includes('rejectedShares: number')) {
  addressSettingsEntitySource = replaceOrThrow(
    addressSettingsEntitySource,
    /(\n\s+@Column\(\{ default: 0 \}\)\s+shares: number;)/,
`$1

    @Column({ default: 0 })
    rejectedShares: number;`,
    'insert AddressSettingsEntity.rejectedShares');
  fs.writeFileSync(addressSettingsEntity, addressSettingsEntitySource);
}

const clientController = path.join(backend, 'src/controllers/client/client.controller.ts');
let clientControllerSource = fs.readFileSync(clientController, 'utf8');
if (!clientControllerSource.includes('rejectedShares: addressSettings?.rejectedShares')) {
  clientControllerSource = clientControllerSource.replace(
`                ...accounting,
                bestSubmissionDifficulty: addressSettings?.bestDifficulty ?? accounting?.bestSubmissionDifficulty ?? 0`,
`                ...accounting,
                rejectedShares: addressSettings?.rejectedShares ?? 0,
                bestSubmissionDifficulty: addressSettings?.bestDifficulty ?? accounting?.bestSubmissionDifficulty ?? 0`);
  fs.writeFileSync(clientController, clientControllerSource);
}

const clientStatisticsEntity = path.join(backend, 'src/ORM/client-statistics/client-statistics.entity.ts');
let clientStatisticsEntitySource = fs.readFileSync(clientStatisticsEntity, 'utf8');
if (!clientStatisticsEntitySource.includes('rejectedCount: number')) {
  clientStatisticsEntitySource = replaceOrThrow(
    clientStatisticsEntitySource,
    /(\n\s+@Column\(\{ default: 0, type: 'integer' \}\)\s+acceptedCount: number;)/,
`$1

    @Column({ default: 0, type: 'integer' })
    rejectedCount: number;`,
    'insert ClientStatisticsEntity.rejectedCount');
  fs.writeFileSync(clientStatisticsEntity, clientStatisticsEntitySource);
}

const clientStatisticsService = path.join(backend, 'src/ORM/client-statistics/client-statistics.service.ts');
let clientStatisticsServiceSource = fs.readFileSync(clientStatisticsService, 'utf8');
if (!clientStatisticsServiceSource.includes('rejectedSharesLast10Minutes')) {
  clientStatisticsServiceSource = clientStatisticsServiceSource
    .replace(
`                shares: clientStatistic.shares,
                acceptedCount: clientStatistic.acceptedCount,
                updatedAt: new Date()`,
`                shares: clientStatistic.shares,
                acceptedCount: clientStatistic.acceptedCount,
                rejectedCount: clientStatistic.rejectedCount,
                updatedAt: new Date()`)
    .replaceAll(
`                COALESCE(SUM(acceptedCount), 0) AS totalAcceptedShares,
                COALESCE(SUM(shares), 0) AS totalCreditedDifficulty,
                COALESCE(SUM(CASE WHEN time > ? THEN acceptedCount ELSE 0 END), 0) AS acceptedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN shares ELSE 0 END), 0) AS creditedDifficultyLastHour,`,
`                COALESCE(SUM(acceptedCount), 0) AS totalAcceptedShares,
                COALESCE(SUM(rejectedCount), 0) AS totalRejectedShares,
                COALESCE(SUM(shares), 0) AS totalCreditedDifficulty,
                COALESCE(SUM(CASE WHEN time > ? THEN acceptedCount ELSE 0 END), 0) AS acceptedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN rejectedCount ELSE 0 END), 0) AS rejectedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN shares ELSE 0 END), 0) AS creditedDifficultyLastHour,`)
    .replace('query, [tenMinutesAgo, oneHourAgo, oneHourAgo])', 'query, [tenMinutesAgo, tenMinutesAgo, oneHourAgo, oneHourAgo])')
    .replace('query, [tenMinutesAgo, oneHourAgo, address])', 'query, [tenMinutesAgo, tenMinutesAgo, oneHourAgo, address])')
    .replace('query, [tenMinutesAgo, oneHourAgo, address, clientName])', 'query, [tenMinutesAgo, tenMinutesAgo, oneHourAgo, address, clientName])')
    .replace('query, [tenMinutesAgo, oneHourAgo, address, clientName, sessionId])', 'query, [tenMinutesAgo, tenMinutesAgo, oneHourAgo, address, clientName, sessionId])');
  if (!clientStatisticsServiceSource.includes('rejectedSharesLast10Minutes')) {
    const replaceMethod = (source, signature, replacement) => {
      const start = source.indexOf(signature);
      if (start === -1) return null;
      const open = source.indexOf('{', start);
      if (open === -1) {
        throw new Error(`Public Pool patch failed: missing method body for ${signature}`);
      }
      let depth = 0;
      for (let i = open; i < source.length; i++) {
        if (source[i] === '{') {
          depth++;
        } else if (source[i] === '}') {
          depth--;
          if (depth === 0) {
            return source.slice(0, start) + replacement + source.slice(i + 1);
          }
        }
      }
      throw new Error(`Public Pool patch failed: unterminated method body for ${signature}`);
    };
    const accountingMethods = `
    public async getAccountingForSite() {
        const tenMinutesAgo = Date.now() - (10 * 60 * 1000);
        const oneHourAgo = Date.now() - (60 * 60 * 1000);
        const query = \`
            SELECT
                COALESCE(SUM(acceptedCount), 0) AS totalAcceptedShares,
                COALESCE(SUM(rejectedCount), 0) AS totalRejectedShares,
                COALESCE(SUM(shares), 0) AS totalCreditedDifficulty,
                COALESCE(SUM(CASE WHEN time > ? THEN acceptedCount ELSE 0 END), 0) AS acceptedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN rejectedCount ELSE 0 END), 0) AS rejectedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN shares ELSE 0 END), 0) AS creditedDifficultyLastHour,
                COALESCE(MAX(shares), 0) AS bestSubmissionDifficulty,
                COALESCE((SUM(CASE WHEN time > ? THEN shares ELSE 0 END) * 4294967296) / 3600, 0) AS hashRateLastHour
            FROM client_statistics_entity;
        \`;
        const result = await this.clientStatisticsRepository.query(query, [tenMinutesAgo, tenMinutesAgo, oneHourAgo, oneHourAgo]);
        return result[0];
    }

    public async getAccountingForAddress(address: string) {
        const tenMinutesAgo = Date.now() - (10 * 60 * 1000);
        const oneHourAgo = Date.now() - (60 * 60 * 1000);
        const query = \`
            SELECT
                COALESCE(SUM(acceptedCount), 0) AS totalAcceptedShares,
                COALESCE(SUM(rejectedCount), 0) AS totalRejectedShares,
                COALESCE(SUM(shares), 0) AS totalCreditedDifficulty,
                COALESCE(SUM(CASE WHEN time > ? THEN acceptedCount ELSE 0 END), 0) AS acceptedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN rejectedCount ELSE 0 END), 0) AS rejectedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN shares ELSE 0 END), 0) AS creditedDifficultyLastHour,
                COALESCE(MAX(shares), 0) AS bestSubmissionDifficulty
            FROM client_statistics_entity
            WHERE address = ?;
        \`;
        const result = await this.clientStatisticsRepository.query(query, [tenMinutesAgo, tenMinutesAgo, oneHourAgo, address]);
        return result[0];
    }

    public async getAccountingForGroup(address: string, clientName: string) {
        const tenMinutesAgo = Date.now() - (10 * 60 * 1000);
        const oneHourAgo = Date.now() - (60 * 60 * 1000);
        const query = \`
            SELECT
                COALESCE(SUM(acceptedCount), 0) AS totalAcceptedShares,
                COALESCE(SUM(rejectedCount), 0) AS totalRejectedShares,
                COALESCE(SUM(shares), 0) AS totalCreditedDifficulty,
                COALESCE(SUM(CASE WHEN time > ? THEN acceptedCount ELSE 0 END), 0) AS acceptedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN rejectedCount ELSE 0 END), 0) AS rejectedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN shares ELSE 0 END), 0) AS creditedDifficultyLastHour,
                COALESCE(MAX(shares), 0) AS bestSubmissionDifficulty
            FROM client_statistics_entity
            WHERE address = ? AND clientName = ?;
        \`;
        const result = await this.clientStatisticsRepository.query(query, [tenMinutesAgo, tenMinutesAgo, oneHourAgo, address, clientName]);
        return result[0];
    }

    public async getAccountingForSession(address: string, clientName: string, sessionId: string) {
        const tenMinutesAgo = Date.now() - (10 * 60 * 1000);
        const oneHourAgo = Date.now() - (60 * 60 * 1000);
        const query = \`
            SELECT
                COALESCE(SUM(acceptedCount), 0) AS totalAcceptedShares,
                COALESCE(SUM(rejectedCount), 0) AS totalRejectedShares,
                COALESCE(SUM(shares), 0) AS totalCreditedDifficulty,
                COALESCE(SUM(CASE WHEN time > ? THEN acceptedCount ELSE 0 END), 0) AS acceptedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN rejectedCount ELSE 0 END), 0) AS rejectedSharesLast10Minutes,
                COALESCE(SUM(CASE WHEN time > ? THEN shares ELSE 0 END), 0) AS creditedDifficultyLastHour,
                COALESCE(MAX(shares), 0) AS bestSubmissionDifficulty
            FROM client_statistics_entity
            WHERE address = ? AND clientName = ? AND sessionId = ?;
        \`;
        const result = await this.clientStatisticsRepository.query(query, [tenMinutesAgo, tenMinutesAgo, oneHourAgo, address, clientName, sessionId]);
        return result[0];
    }
`;
    if (clientStatisticsServiceSource.includes('getAccountingForSite()')) {
      for (const signature of [
        '    public async getAccountingForSession(address: string, clientName: string, sessionId: string)',
        '    public async getAccountingForGroup(address: string, clientName: string)',
        '    public async getAccountingForAddress(address: string)',
      ]) {
        const updated = replaceMethod(clientStatisticsServiceSource, signature, '');
        if (updated != null) {
          clientStatisticsServiceSource = updated;
        }
      }
      clientStatisticsServiceSource = replaceMethod(
        clientStatisticsServiceSource,
        '    public async getAccountingForSite()',
        accountingMethods
      );
    } else {
      clientStatisticsServiceSource = replaceOrThrow(
        clientStatisticsServiceSource,
        /\n\s+public async deleteAll\(\) \{/,
        `\n${accountingMethods}\n    public async deleteAll() {`,
        'insert ClientStatisticsService accounting methods');
    }
  }
  fs.writeFileSync(clientStatisticsService, clientStatisticsServiceSource);
}

const clientStatistics = path.join(backend, 'src/models/StratumV1ClientStatistics.ts');
let clientStatisticsSource = fs.readFileSync(clientStatistics, 'utf8');
if (!clientStatisticsSource.includes('private rejectedCount: number = 0;')) {
  clientStatisticsSource = replaceOrThrow(
    clientStatisticsSource,
    /(\n\s+private acceptedCount: number = 0;\r?\n)/,
    `$1    private rejectedCount: number = 0;\n`,
    'insert StratumV1ClientStatistics.rejectedCount field');
}
clientStatisticsSource = clientStatisticsSource.replaceAll(
  '                acceptedCount: this.acceptedCount,\n                address: client.address,',
  '                acceptedCount: this.acceptedCount,\n                rejectedCount: this.rejectedCount,\n                address: client.address,');
clientStatisticsSource = clientStatisticsSource.replace(
  '            this.acceptedCount = 1\n            await this.clientStatisticsService.insert({',
  '            this.acceptedCount = 1\n            this.rejectedCount = 0;\n            await this.clientStatisticsService.insert({');
if (!clientStatisticsSource.includes('public async addRejectedShare(client: ClientEntity)')) {
  clientStatisticsSource = replaceOrThrow(
    clientStatisticsSource,
    /\n\s+public getSuggestedDifficulty\(clientDifficulty: number\) \{/,
`\n    public async addRejectedShare(client: ClientEntity) {
        const coeff = 1000 * 60 * 10;
        const date = new Date();
        const timeSlot = new Date(Math.floor(date.getTime() / coeff) * coeff).getTime();

        if (this.currentTimeSlot == null) {
            this.previousTimeSlotTime = new Date();
            this.currentTimeSlotTime = new Date();
            this.currentTimeSlot = timeSlot;
            this.rejectedCount++;
            await this.clientStatisticsService.insert({
                time: this.currentTimeSlot,
                shares: this.shares,
                acceptedCount: this.acceptedCount,
                rejectedCount: this.rejectedCount,
                address: client.address,
                clientName: client.clientName,
                sessionId: client.sessionId
            });
            this.lastSave = new Date().getTime();
        } else if (this.currentTimeSlot != timeSlot) {
            await this.clientStatisticsService.update({
                time: this.currentTimeSlot,
                shares: this.shares,
                acceptedCount: this.acceptedCount,
                rejectedCount: this.rejectedCount,
                address: client.address,
                clientName: client.clientName,
                sessionId: client.sessionId
            });
            this.previousShares = this.shares;
            this.previousTimeSlotTime = this.currentTimeSlotTime;
            this.currentTimeSlotTime = new Date();
            this.currentTimeSlot = timeSlot;
            this.shares = 0;
            this.acceptedCount = 0;
            this.rejectedCount = 1;
            await this.clientStatisticsService.insert({
                time: this.currentTimeSlot,
                shares: this.shares,
                acceptedCount: this.acceptedCount,
                rejectedCount: this.rejectedCount,
                address: client.address,
                clientName: client.clientName,
                sessionId: client.sessionId
            });
            this.lastSave = new Date().getTime();
        } else {
            this.rejectedCount++;
            await this.clientStatisticsService.update({
                time: this.currentTimeSlot,
                shares: this.shares,
                acceptedCount: this.acceptedCount,
                rejectedCount: this.rejectedCount,
                address: client.address,
                clientName: client.clientName,
                sessionId: client.sessionId
            });
            this.lastSave = new Date().getTime();
        }
    }

    public getSuggestedDifficulty(clientDifficulty: number) {`,
    'insert StratumV1ClientStatistics.addRejectedShare');
}
fs.writeFileSync(clientStatistics, clientStatisticsSource);

const stratumClient = path.join(backend, 'src/models/StratumV1Client.ts');
let stratumClientSource = fs.readFileSync(stratumClient, 'utf8');
if (!stratumClientSource.includes('private async recordRejectedShare()')) {
  stratumClientSource = stratumClientSource.replace(
`    private async handleMiningSubmission(submission: MiningSubmitMessage) {`,
`    private async recordRejectedShare() {
        if (!this.clientAuthorization?.address) {
            return;
        }
        try {
            await this.addressSettingsService.addRejectedShare(this.clientAuthorization.address);
        } catch (e) {
            console.log(e);
        }
    }

    private async handleMiningSubmission(submission: MiningSubmitMessage) {`);
  stratumClientSource = stratumClientSource.replaceAll(
`            const success = await this.write(err);
            if (!success) {
                return false;
            }
            return false;`,
`            const success = await this.write(err);
            await this.recordRejectedShare();
            if (!success) {
                return false;
            }
            return false;`);
  fs.writeFileSync(stratumClient, stratumClientSource);
}
if (stratumClientSource.includes('private async recordRejectedShare()') && !stratumClientSource.includes('await this.statistics.addRejectedShare(this.entity);')) {
  stratumClientSource = stratumClientSource.replace(
`        try {
            await this.addressSettingsService.addRejectedShare(this.clientAuthorization.address);
        } catch (e) {`,
`        try {
            await this.ensureClientEntity();
            await this.statistics.addRejectedShare(this.entity);
            await this.addressSettingsService.addRejectedShare(this.clientAuthorization.address);
        } catch (e) {`);
  fs.writeFileSync(stratumClient, stratumClientSource);
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

const requiredBackendPatches = [
  [addressSettingsService, 'public async addRejectedShare(address: string)'],
  [addressSettingsEntity, 'rejectedShares: number'],
  [clientStatisticsEntity, 'rejectedCount: number'],
  [clientStatisticsService, 'rejectedSharesLast10Minutes'],
  [clientStatistics, 'private rejectedCount: number = 0;'],
  [clientStatistics, 'public async addRejectedShare(client: ClientEntity)'],
  [stratumClient, 'await this.statistics.addRejectedShare(this.entity);'],
  [appController, "@Get('info/addresses')"],
];
for (const [file, needle] of requiredBackendPatches) {
  if (!fs.readFileSync(file, 'utf8').includes(needle)) {
    throw new Error(`Public Pool patch failed: ${path.relative(backend, file)} is missing ${needle}`);
  }
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
if (!splashTemplateSource.includes('Rejected {{(entry.rejectedShares || 0) | numberSuffix}}')) {
  splashTemplateSource = splashTemplateSource.replace(
`                            <span>{{(entry.shares || 0) | numberSuffix}} shares</span>
                            <span>Best {{(entry.bestDifficulty || 0) | numberSuffix}}</span>`,
`                            <span>{{(entry.shares || 0) | numberSuffix}} shares</span>
                            <span>Rejected {{(entry.rejectedShares || 0) | numberSuffix}}</span>
                            <span>Best {{(entry.bestDifficulty || 0) | numberSuffix}}</span>`);
  fs.writeFileSync(splashTemplate, splashTemplateSource);
}

const splashStyles = path.join(frontend, 'src/app/components/splash/splash.component.scss');
let splashStylesSource = fs.readFileSync(splashStyles, 'utf8');
if (!splashStylesSource.includes('.address-list')) {
  splashStylesSource = splashStylesSource.replace(
`.round-card {
    padding: 1.5rem;
}`,
`.address-list {
    border: 1px solid var(--surface-border);
    margin-top: 1rem;
    overflow: hidden;
}

.address-row {
    align-items: center;
    display: grid;
    gap: 1rem;
    grid-template-columns: minmax(0, 1fr) auto auto auto;
    padding: 0.85rem 1rem;
    text-decoration: none;
    color: var(--text-color);
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

FRONTEND_SRC="$FRONTEND_SRC" "$NODE_BIN" <<'JS'
const fs = require('fs');
const path = require('path');

const frontend = process.env.FRONTEND_SRC;
const dashboardTemplate = path.join(frontend, 'src/app/components/dashboard/dashboard.component.html');
let dashboardSource = fs.readFileSync(dashboardTemplate, 'utf8');
if (!dashboardSource.includes('<span>Rejected Shares</span>')) {
  dashboardSource = dashboardSource.replace(
`                    <div class="snapshot-metric">
                        <span>Accepted Shares</span>
                        <strong>{{clientInfo.accounting?.totalAcceptedShares | numberSuffix}}</strong>
                        <small>{{clientInfo.accounting?.acceptedSharesLast10Minutes | numberSuffix}} last 10m</small>
                    </div>

                    <div class="snapshot-metric">
                        <span>Accumulated Work</span>`,
`                    <div class="snapshot-metric">
                        <span>Accepted Shares</span>
                        <strong>{{clientInfo.accounting?.totalAcceptedShares | numberSuffix}}</strong>
                        <small>{{clientInfo.accounting?.acceptedSharesLast10Minutes | numberSuffix}} last 10m</small>
                    </div>

                    <div class="snapshot-metric">
                        <span>Rejected Shares</span>
                        <strong>{{(clientInfo.accounting?.rejectedShares || 0) | numberSuffix}}</strong>
                        <small>{{(clientInfo.accounting?.rejectedSharesLast10Minutes || 0) | numberSuffix}} last 10m</small>
                    </div>

                    <div class="snapshot-metric">
                        <span>Accumulated Work</span>`);
  fs.writeFileSync(dashboardTemplate, dashboardSource);
}
dashboardSource = fs.readFileSync(dashboardTemplate, 'utf8');
if (dashboardSource.includes('<small>Rejected</small>')) {
  dashboardSource = dashboardSource.replace('<small>Rejected</small>', '<small>{{(clientInfo.accounting?.rejectedSharesLast10Minutes || 0) | numberSuffix}} last 10m</small>');
  fs.writeFileSync(dashboardTemplate, dashboardSource);
}

const dashboardStyles = path.join(frontend, 'src/app/components/dashboard/dashboard.component.scss');
let dashboardStylesSource = fs.readFileSync(dashboardStyles, 'utf8');
dashboardStylesSource = dashboardStylesSource
  .replace('gap: 1.25rem;\n    grid-template-columns: minmax(13rem, 1.35fr) repeat(3, minmax(0, 1fr));', 'gap: 0.55rem;\n    grid-template-columns: minmax(8rem, 1.15fr) repeat(4, minmax(5.75rem, 1fr));')
  .replace('gap: 0.85rem;\n    grid-template-columns: minmax(10rem, 1.2fr) repeat(4, minmax(0, 1fr));', 'gap: 0.55rem;\n    grid-template-columns: minmax(8rem, 1.15fr) repeat(4, minmax(5.75rem, 1fr));')
  .replace('font-size: 0.82rem;', 'font-size: 0.72rem;')
  .replace('font-size: 0.78rem;', 'font-size: 0.72rem;')
  .replace('font-size: 1.35rem;\n    font-weight: 600;\n    margin: 0.45rem 0 0.25rem;', 'font-size: 1.08rem;\n    font-weight: 600;\n    margin: 0.45rem 0 0.25rem;')
  .replace('font-size: 1.18rem;\n    font-weight: 600;\n    margin: 0.45rem 0 0.25rem;', 'font-size: 1.08rem;\n    font-weight: 600;\n    margin: 0.45rem 0 0.25rem;')
  .replace('font-size: 1.65rem;', 'font-size: 1.28rem;')
  .replace('font-size: 1.45rem;', 'font-size: 1.28rem;')
  .replace('    .hero-stats,\n    .snapshot-grid {\n        grid-template-columns: repeat(2, minmax(0, 1fr));\n    }', '    .hero-stats {\n        grid-template-columns: repeat(2, minmax(0, 1fr));\n    }');
if (!dashboardStylesSource.includes('.snapshot-metric span {\n    white-space: nowrap;\n}')) {
  dashboardStylesSource = dashboardStylesSource.replace('.snapshot-metric strong {', '.snapshot-metric span {\n    white-space: nowrap;\n}\n\n.snapshot-metric strong {');
}
fs.writeFileSync(dashboardStyles, dashboardStylesSource);
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
