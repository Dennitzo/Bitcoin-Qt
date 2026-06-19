param(
    [string]$Version = $env:MARIADB_VERSION,
    [string]$RuntimeRoot = $env:BITCOIN_QT_RUNTIME_ROOT,
    [switch]$ForceDownload
)

. (Join-Path $PSScriptRoot 'common.ps1')

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = '11.4.12'
}

$root = Get-ProjectRoot
$runtimeRoot = Resolve-RuntimeRoot -RuntimeRoot $RuntimeRoot
$downloadDir = Join-Path $root 'build\runtime-downloads'
$extractDir = Join-Path $root "build\runtime-src\mariadb-$Version-winx64"
$prefix = Join-Path $runtimeRoot 'mariadb'
$archiveName = "mariadb-$Version-winx64.zip"
$archivePath = Join-Path $downloadDir $archiveName
$archiveUrl = "https://archive.mariadb.org/mariadb-$Version/winx64-packages/$archiveName"

Write-Step "Staging MariaDB $Version portable runtime for Windows x64"
Download-File -Uri $archiveUrl -OutFile $archivePath -Force:$ForceDownload
Expand-ZipFile -Archive $archivePath -Destination $extractDir
Remove-SafeDirectory $prefix

$sourceRoot = Join-Path $extractDir "mariadb-$Version-winx64"
if (!(Test-Path -LiteralPath (Join-Path $sourceRoot 'bin\mariadbd.exe'))) {
    throw "Expected MariaDB archive layout not found: $sourceRoot"
}
if (!(Test-Path -LiteralPath (Join-Path $sourceRoot 'bin\mariadb-install-db.exe'))) {
    throw "Expected MariaDB initializer not found in archive: bin\mariadb-install-db.exe"
}

Copy-Item -LiteralPath $sourceRoot -Destination $prefix -Recurse -Force
Set-Content -LiteralPath (Join-Path $prefix 'VERSION') -Value $Version -Encoding ASCII
Invoke-Tool (Join-Path $prefix 'bin\mariadbd.exe') '--version'
