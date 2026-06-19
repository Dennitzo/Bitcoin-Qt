param(
    [string]$Version = $env:BITCOIN_CORE_VERSION,
    [string]$Architecture = 'win64',
    [string]$RuntimeRoot = $env:BITCOIN_QT_RUNTIME_ROOT,
    [switch]$SkipChecksum,
    [switch]$ForceDownload
)

. (Join-Path $PSScriptRoot 'common.ps1')

if ($Architecture -ne 'win64') {
    throw "Only Windows x64 Bitcoin Core archives are supported by this script."
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Get-LatestBitcoinCoreVersion
}

$root = Get-ProjectRoot
$runtimeRoot = Resolve-RuntimeRoot -RuntimeRoot $RuntimeRoot
$downloadDir = Join-Path $root 'build\runtime-downloads'
$extractDir = Join-Path $root "build\runtime-src\bitcoin-$Version-$Architecture"
$prefix = Join-Path $runtimeRoot 'bitcoin'
$archiveName = "bitcoin-$Version-$Architecture.zip"
$archivePath = Join-Path $downloadDir $archiveName
$baseUrl = "https://bitcoincore.org/bin/bitcoin-core-$Version"
$archiveUrl = "$baseUrl/$archiveName"
$sumsPath = Join-Path $downloadDir "bitcoin-core-$Version-SHA256SUMS"

Write-Step "Staging Bitcoin Core $Version for Windows x64"
Download-File -Uri $archiveUrl -OutFile $archivePath -Force:$ForceDownload

if (!$SkipChecksum) {
    Download-File -Uri "$baseUrl/SHA256SUMS" -OutFile $sumsPath -Force:$ForceDownload
    $sumLine = Get-Content -LiteralPath $sumsPath |
        Where-Object { $_ -match "(^|[ \t])\*?$([regex]::Escape($archiveName))$" } |
        Select-Object -First 1
    if (!$sumLine) {
        throw "Could not find $archiveName in SHA256SUMS"
    }
    $expectedHash = (($sumLine -split '\s+')[0]).ToLowerInvariant()
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash.ToLowerInvariant()
    if ($expectedHash -ne $actualHash) {
        throw "SHA256 mismatch for $archiveName. Expected $expectedHash, got $actualHash"
    }
    Write-Host "SHA256 verified: $actualHash"
}

Expand-ZipFile -Archive $archivePath -Destination $extractDir
Remove-SafeDirectory $prefix
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'bin') | Out-Null

$sourceBin = Join-Path $extractDir "bitcoin-$Version\bin"
foreach ($binary in @('bitcoind.exe', 'bitcoin-cli.exe')) {
    $source = Join-Path $sourceBin $binary
    if (!(Test-Path -LiteralPath $source)) {
        throw "Expected Bitcoin Core binary not found: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $prefix "bin\$binary") -Force
}

Set-Content -LiteralPath (Join-Path $prefix 'VERSION') -Value $Version -Encoding ASCII
Invoke-Tool (Join-Path $prefix 'bin\bitcoind.exe') '--version'
