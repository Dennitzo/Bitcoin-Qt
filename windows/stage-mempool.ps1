param(
    [string]$RuntimeRoot = $env:BITCOIN_QT_RUNTIME_ROOT,
    [string]$Ref = $env:MEMPOOL_REF,
    [switch]$ForceUpdate
)

. (Join-Path $PSScriptRoot 'common.ps1')

$root = Get-ProjectRoot
$runtimeRoot = Resolve-RuntimeRoot -RuntimeRoot $RuntimeRoot
$buildDir = Join-Path $root 'build\runtime-src'
$sourceDir = Join-Path $buildDir 'mempool'
$prefix = Join-Path $runtimeRoot 'mempool'
$node = Join-Path $runtimeRoot 'node\bin\node.exe'
$npm = Join-Path $runtimeRoot 'node\bin\npm.cmd'
$git = Resolve-GitExecutable

if ([string]::IsNullOrWhiteSpace($Ref)) {
    $Ref = 'master'
}

Write-Step "Staging Mempool $Ref for native Windows runtime"

if (!(Test-Path -LiteralPath $node) -or !(Test-Path -LiteralPath $npm)) {
    throw "Node runtime missing below $runtimeRoot. Run windows\stage-node-runtime.ps1 first."
}
if (!$git) {
    throw "Git for Windows was not found. Install Git, or make git.exe available on PATH."
}
if (!(Get-Command cargo.exe -ErrorAction SilentlyContinue)) {
    throw "cargo.exe was not found. Install Rust with the x86_64-pc-windows-msvc toolchain."
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
if (!(Test-Path -LiteralPath (Join-Path $sourceDir '.git'))) {
    Invoke-Tool $git 'clone' 'https://github.com/mempool/mempool.git' $sourceDir
} else {
    Write-Host "Using existing checkout: $sourceDir"
}

if ($ForceUpdate) {
    Invoke-Tool $git '-C' $sourceDir 'fetch' '--tags' '--prune'
}
Invoke-Tool $git '-C' $sourceDir 'checkout' $Ref
Invoke-Tool $git '-C' $sourceDir 'checkout' $Ref '--' `
    'backend/package-lock.json' `
    'backend/package.json' `
    'backend/src/repositories/HashratesRepository.ts' `
    'frontend/package-lock.json' `
    'frontend/package.json' `
    'frontend/src/app/components/difficulty/difficulty.component.ts' `
    'frontend/src/app/components/hashrate-chart/hashrate-chart.component.ts' `
    'rust/gbt/package.json'

if ([string]::IsNullOrWhiteSpace($env:CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER)) {
    $link = Resolve-Tool -Name 'link.exe'
    if (!$link) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path -LiteralPath $vswhere) {
            $installations = & $vswhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            $link = $installations |
                ForEach-Object {
                    Get-ChildItem -LiteralPath (Join-Path $_ 'VC\Tools\MSVC') -Recurse -Filter link.exe -ErrorAction SilentlyContinue
                } |
                Where-Object { $_.FullName -like '*\bin\Hostx64\x64\link.exe' } |
                Sort-Object FullName -Descending |
                Select-Object -First 1 -ExpandProperty FullName
        }
    }
    if (!$link) {
        throw 'MSVC link.exe was not found. Install the Visual Studio Desktop development with C++ workload.'
    }
    $env:CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER = $link
}

function Invoke-MempoolNodeScript {
    param([Parameter(Mandatory = $true)][string]$Script)
    $scriptPath = Join-Path $buildDir 'mempool-windows-build.js'
    Set-Content -LiteralPath $scriptPath -Value $Script -Encoding UTF8
    Push-Location $sourceDir
    try {
        Invoke-Tool $node $scriptPath
    } finally {
        Pop-Location
        Remove-Item -LiteralPath $scriptPath -Force -ErrorAction SilentlyContinue
    }
}

Write-Step 'Applying native Windows build compatibility patches'
Invoke-MempoolNodeScript -Script @'
const fs = require('fs');

const gbtPath = 'rust/gbt/package.json';
const gbt = JSON.parse(fs.readFileSync(gbtPath, 'utf8'));
gbt.scripts['check-cargo-version'] = 'cargo version';
gbt.scripts['build-release'] = 'npm run build -- --release';
gbt.scripts['to-backend'] = 'node copy-to-backend.js';
fs.writeFileSync(gbtPath, `${JSON.stringify(gbt, null, 2)}\n`);
fs.writeFileSync('rust/gbt/copy-to-backend.js', `
const fs = require('fs');
const path = require('path');
const dest = path.resolve(__dirname, '../../backend/rust-gbt');
fs.rmSync(dest, {recursive: true, force: true});
fs.mkdirSync(dest, {recursive: true});
for (const name of ['index.js', 'index.d.ts', 'package.json']) {
  fs.copyFileSync(path.join(__dirname, name), path.join(dest, name));
}
for (const name of fs.readdirSync(__dirname)) {
  if (name.endsWith('.node')) fs.copyFileSync(path.join(__dirname, name), path.join(dest, name));
}
`);

for (const file of ['backend/package.json', 'frontend/package.json']) {
  const pkg = JSON.parse(fs.readFileSync(file, 'utf8'));
  for (const [name, script] of Object.entries(pkg.scripts || {})) {
    pkg.scripts[name] = script
      .replaceAll('./node_modules/typescript/bin/tsc', 'node ./node_modules/typescript/bin/tsc')
      .replaceAll('./node_modules/@angular/cli/bin/ng.js', 'node ./node_modules/@angular/cli/bin/ng.js')
      .replaceAll('./node_modules/.bin/eslint', 'eslint')
      .replaceAll('./node_modules/.bin/jest', 'jest');
  }
  if (file === 'backend/package.json') pkg.scripts['create-resources'] = 'node copy-resources.js';
  if (file === 'frontend/package.json') pkg.scripts['sync-assets'] = 'npm run copy-themes && node copy-resources.js';
  fs.writeFileSync(file, `${JSON.stringify(pkg, null, 2)}\n`);
}

fs.writeFileSync('backend/copy-resources.js', `
const fs = require('fs');
fs.mkdirSync('dist/tasks', {recursive: true});
fs.copyFileSync('src/tasks/price-feeds/mtgox-weekly.json', 'dist/tasks/mtgox-weekly.json');
require('./dist/api/fetch-version.js');
`);

fs.writeFileSync('frontend/copy-resources.js', `
const fs = require('fs');
const path = require('path');
const source = path.resolve(__dirname, 'src/resources');
const dest = path.resolve(__dirname, 'dist/mempool/browser/resources');
fs.rmSync(dest, {recursive: true, force: true});
fs.mkdirSync(path.dirname(dest), {recursive: true});
fs.cpSync(source, dest, {recursive: true});
`);

const difficultyPath = 'frontend/src/app/components/difficulty/difficulty.component.ts';
let difficulty = fs.readFileSync(difficultyPath, 'utf8');
if (!difficulty.includes('if (start > end)')) {
  difficulty = difficulty.replace(
`    if (startX > endX) {
      return [];
    }`,
`    if (start > end || startX > endX) {
      return [];
    }`);
  fs.writeFileSync(difficultyPath, difficulty);
}

const chartPath = 'frontend/src/app/components/hashrate-chart/hashrate-chart.component.ts';
let chart = fs.readFileSync(chartPath, 'utf8');
chart = chart.replace('Indexing blocks`,' , 'Indexing network hashrate`,');
fs.writeFileSync(chartPath, chart);

const repositoryPath = 'backend/src/repositories/HashratesRepository.ts';
let repository = fs.readFileSync(repositoryPath, 'utf8');
if (!repository.includes('NO_AUTO_VALUE_ON_ZERO')) {
  repository = repository.replace(
`    let query = \`INSERT INTO
      hashrates(hashrate_timestamp, avg_hashrate, pool_id, share, type) VALUES\`;`,
`    if (hashrates.some((hashrate) => hashrate.poolId === 0)) {
      await DB.query(\`SET SESSION sql_mode = IF(
        FIND_IN_SET('NO_AUTO_VALUE_ON_ZERO', @@SESSION.sql_mode),
        @@SESSION.sql_mode,
        CONCAT_WS(',', @@SESSION.sql_mode, 'NO_AUTO_VALUE_ON_ZERO')
      )\`);
      await DB.query(\`
        INSERT IGNORE INTO pools(id, name, link, addresses, regexes, slug, unique_id)
        VALUES (0, 'Network', '', '[]', '[]', 'network', -2)
      \`);
    }

    let query = \`INSERT INTO
      hashrates(hashrate_timestamp, avg_hashrate, pool_id, share, type) VALUES\`;`);
  fs.writeFileSync(repositoryPath, repository);
}
'@

foreach ($package in @('backend', 'frontend')) {
    $packageDir = Join-Path $sourceDir $package
    Write-Step "Installing and building Mempool $package"
    Push-Location $packageDir
    try {
        Invoke-Tool $npm 'ci'
        if ($package -eq 'frontend') {
            $previousSkipSync = $env:SKIP_SYNC
            $env:SKIP_SYNC = '1'
            try {
                Invoke-Tool $npm 'run' 'build'
            } finally {
                $env:SKIP_SYNC = $previousSkipSync
            }
        } else {
            Invoke-Tool $npm 'run' 'build'
        }
    } finally {
        Pop-Location
    }
}

Invoke-MempoolNodeScript -Script @'
const fs = require('fs');
const file = 'backend/dist/api/database-migration.js';
if (!fs.existsSync(file)) process.exit(0);
let source = fs.readFileSync(file, 'utf8');
source = source.replaceAll(
  "ALTER TABLE blocks DROP FOREIGN KEY IF EXISTS `blocks_ibfk_1`",
  "ALTER TABLE blocks DROP FOREIGN KEY IF EXISTS `blocks_ibfk_1`');\n      await this.$executeQuery('ALTER TABLE blocks DROP FOREIGN KEY IF EXISTS `1`");
source = source.replaceAll(
  "ALTER TABLE `hashrates` DROP FOREIGN KEY `hashrates_ibfk_1`",
  "ALTER TABLE `hashrates` DROP FOREIGN KEY IF EXISTS `hashrates_ibfk_1`");
source = source.replaceAll(
  "await this.$executeQuery('START TRANSACTION;');",
  "await this.$executeQuery('SET FOREIGN_KEY_CHECKS=0;');\n            await this.$executeQuery('START TRANSACTION;')");
source = source.replaceAll(
  "await this.$executeQuery('COMMIT;');",
  "await this.$executeQuery('COMMIT;');\n            await this.$executeQuery('SET FOREIGN_KEY_CHECKS=1;')");
source = source.replaceAll(
  "await this.$executeQuery('ROLLBACK;');",
  "await this.$executeQuery('ROLLBACK;');\n            await this.$executeQuery('SET FOREIGN_KEY_CHECKS=1;')");
source = source.replaceAll(
  "await this.$createMissingTablesAndIndexes(databaseSchemaVersion);",
  "await this.$executeQuery('SET FOREIGN_KEY_CHECKS=0;');\n            await this.$createMissingTablesAndIndexes(databaseSchemaVersion);\n            await this.$executeQuery('SET FOREIGN_KEY_CHECKS=1;')");
fs.writeFileSync(file, source);
'@

Remove-SafeDirectory $prefix
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'backend') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'frontend\dist') | Out-Null

Write-Step 'Copying Mempool runtime files'
Copy-Item -Path (Join-Path $sourceDir 'backend\*') -Destination (Join-Path $prefix 'backend') -Recurse -Force
foreach ($relative in @('.git', 'test', 'tests')) {
    Remove-SafeDirectory (Join-Path $prefix "backend\$relative")
}
Copy-Item -Path (Join-Path $sourceDir 'frontend\dist\*') -Destination (Join-Path $prefix 'frontend\dist') -Recurse -Force

Set-Content -LiteralPath (Join-Path $prefix 'backend\server.js') -Encoding ASCII -Value @'
process.chdir(__dirname);
process.argv = [process.argv[0], require.resolve('./dist/index.js')];
require('./dist/index.js');
'@
Copy-Item -LiteralPath (Join-Path $root 'scripts\mempool-frontend-server.js') -Destination (Join-Path $prefix 'frontend\server.js') -Force

foreach ($entrypoint in @('backend\dist\index.js', 'backend\server.js', 'frontend\server.js')) {
    $candidate = Join-Path $prefix $entrypoint
    if (!(Test-Path -LiteralPath $candidate)) {
        throw "Mempool runtime entrypoint missing after staging: $candidate"
    }
}

Write-Host "Mempool runtime staged at $prefix"
