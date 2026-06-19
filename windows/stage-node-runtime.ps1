param(
    [string]$NodeVersion = $env:NODE_VERSION,
    [string]$RuntimeRoot = $env:BITCOIN_QT_RUNTIME_ROOT,
    [switch]$ForceDownload
)

. (Join-Path $PSScriptRoot 'common.ps1')

if ([string]::IsNullOrWhiteSpace($NodeVersion)) {
    $NodeVersion = 'v24.13.0'
}
if (!$NodeVersion.StartsWith('v')) {
    $NodeVersion = "v$NodeVersion"
}

$root = Get-ProjectRoot
$runtimeRoot = Resolve-RuntimeRoot -RuntimeRoot $RuntimeRoot
$downloadDir = Join-Path $root 'build\runtime-downloads'
$extractDir = Join-Path $root "build\runtime-src\node-$NodeVersion-win-x64"
$prefix = Join-Path $runtimeRoot 'node'
$archiveName = "node-$NodeVersion-win-x64.zip"
$archivePath = Join-Path $downloadDir $archiveName
$archiveUrl = "https://nodejs.org/dist/$NodeVersion/$archiveName"

Write-Step "Staging Node.js $NodeVersion for Windows x64"
Download-File -Uri $archiveUrl -OutFile $archivePath -Force:$ForceDownload
Expand-ZipFile -Archive $archivePath -Destination $extractDir
Remove-SafeDirectory $prefix

$sourceRoot = Join-Path $extractDir "node-$NodeVersion-win-x64"
if (!(Test-Path -LiteralPath (Join-Path $sourceRoot 'node.exe'))) {
    throw "Expected Node.js archive layout not found: $sourceRoot"
}

Copy-Item -LiteralPath $sourceRoot -Destination $prefix -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'bin') | Out-Null
Copy-Item -LiteralPath (Join-Path $prefix 'node.exe') -Destination (Join-Path $prefix 'bin\node.exe') -Force

@'
@echo off
set "DIR=%~dp0.."
"%DIR%\npm.cmd" %*
'@ | Set-Content -LiteralPath (Join-Path $prefix 'bin\npm.cmd') -Encoding ASCII

@'
@echo off
set "DIR=%~dp0.."
"%DIR%\npx.cmd" %*
'@ | Set-Content -LiteralPath (Join-Path $prefix 'bin\npx.cmd') -Encoding ASCII

if (Test-Path -LiteralPath (Join-Path $prefix 'corepack.cmd')) {
    @'
@echo off
set "DIR=%~dp0.."
"%DIR%\corepack.cmd" %*
'@ | Set-Content -LiteralPath (Join-Path $prefix 'bin\corepack.cmd') -Encoding ASCII
}

Set-Content -LiteralPath (Join-Path $prefix 'VERSION') -Value $NodeVersion -Encoding ASCII
Invoke-Tool (Join-Path $prefix 'bin\node.exe') '--version'
