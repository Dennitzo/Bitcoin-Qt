param(
    [switch]$BuildElectrs,
    [string]$RuntimeRoot = $env:BITCOIN_QT_RUNTIME_ROOT,
    [switch]$SkipMariaDB,
    [switch]$SkipPublicPool,
    [switch]$SkipBitcoinChecksum,
    [switch]$ForceDownload
)

. (Join-Path $PSScriptRoot 'common.ps1')

Write-Step "Staging native Windows runtime components"
$resolvedRuntimeRoot = Resolve-RuntimeRoot -RuntimeRoot $RuntimeRoot

& (Join-Path $PSScriptRoot 'stage-bitcoin-core.ps1') -RuntimeRoot $resolvedRuntimeRoot -SkipChecksum:$SkipBitcoinChecksum -ForceDownload:$ForceDownload

& (Join-Path $PSScriptRoot 'stage-node-runtime.ps1') -RuntimeRoot $resolvedRuntimeRoot -ForceDownload:$ForceDownload

if (!$SkipMariaDB) {
    & (Join-Path $PSScriptRoot 'stage-mariadb-runtime.ps1') -RuntimeRoot $resolvedRuntimeRoot -ForceDownload:$ForceDownload
}

if ($BuildElectrs) {
    & (Join-Path $PSScriptRoot 'stage-electrs.ps1') -RuntimeRoot $resolvedRuntimeRoot
} else {
    Write-Warning "electrs has no official Windows x64 binary staged here. Run windows\stage-electrs.ps1, or windows\stage-runtime.ps1 -BuildElectrs, after installing Rust and Git."
}

if (!$SkipPublicPool) {
    & (Join-Path $PSScriptRoot 'stage-public-pool.ps1') -RuntimeRoot $resolvedRuntimeRoot
}

Write-Host ''
Write-Host "Runtime files staged below $resolvedRuntimeRoot"
foreach ($relativePath in @(
    'bitcoin\bin\bitcoind.exe',
    'bitcoin\bin\bitcoin-cli.exe',
    'electrs\bin\electrs.exe',
    'node\bin\node.exe',
    'node\bin\npm.cmd',
    'mariadb\bin\mariadbd.exe',
    'mariadb\bin\mariadb-install-db.exe',
    'public-pool\backend\dist\main.js',
    'public-pool\frontend\server.js'
)) {
    $candidate = Join-Path $resolvedRuntimeRoot $relativePath
    if (Test-Path -LiteralPath $candidate) {
        Write-Host "  $candidate"
    } else {
        Write-Warning "Missing expected runtime file: $candidate"
    }
}
