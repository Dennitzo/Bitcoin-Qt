param(
    [string]$Ref = $env:ELECTRS_REF,
    [string]$RuntimeRoot = $env:BITCOIN_QT_RUNTIME_ROOT
)

. (Join-Path $PSScriptRoot 'common.ps1')

if ([string]::IsNullOrWhiteSpace($Ref)) {
    $Ref = 'master'
}

$git = Resolve-GitExecutable
if (!$git) {
    throw "git.exe not found. Install Git for Windows or add it to PATH."
}

$cargo = Resolve-Tool -Name 'cargo.exe' -Candidates @(
    (Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe')
)
if (!$cargo) {
    throw "cargo.exe not found. Install Rust from https://rustup.rs/."
}

Import-VisualStudioEnvironment

if ([string]::IsNullOrWhiteSpace($env:LIBCLANG_PATH)) {
    foreach ($candidate in @(
        'C:\Program Files\LLVM\bin',
        'C:\Program Files\LLVM\lib'
    )) {
        if ((Test-Path -LiteralPath (Join-Path $candidate 'libclang.dll')) -or
            (Test-Path -LiteralPath (Join-Path $candidate 'clang.dll')) -or
            (Test-Path -LiteralPath (Join-Path $candidate 'libclang.lib'))) {
            $env:LIBCLANG_PATH = $candidate
            Add-PathEntry $candidate
            break
        }
    }
}

$root = Get-ProjectRoot
$runtimeRoot = Resolve-RuntimeRoot -RuntimeRoot $RuntimeRoot
$sourceDir = Join-Path $root 'build\runtime-src\electrs'
$prefix = Join-Path $runtimeRoot 'electrs'

Write-Step "Building electrs $Ref natively for Windows x64"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $sourceDir) | Out-Null
if (!(Test-Path -LiteralPath (Join-Path $sourceDir '.git'))) {
    Invoke-Tool $git 'clone' 'https://github.com/romanz/electrs.git' $sourceDir
}

Invoke-Tool $git '-C' $sourceDir 'fetch' '--tags' '--prune'
Invoke-Tool $git '-C' $sourceDir 'checkout' $Ref
Push-Location $sourceDir
try {
    Invoke-Tool $cargo 'build' '--release' '--locked'
} finally {
    Pop-Location
}

Remove-SafeDirectory $prefix
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'bin') | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceDir 'target\release\electrs.exe') -Destination (Join-Path $prefix 'bin\electrs.exe') -Force
$cargoManifest = Get-Content -LiteralPath (Join-Path $sourceDir 'Cargo.toml') -Raw
$versionMatch = [regex]::Match($cargoManifest, '(?ms)^\[package\].*?^version\s*=\s*"([^"]+)"')
$version = if ($versionMatch.Success) { $versionMatch.Groups[1].Value } else { $Ref }
Set-Content -LiteralPath (Join-Path $prefix 'VERSION') -Value $version -Encoding ASCII
Invoke-Tool (Join-Path $prefix 'bin\electrs.exe') '--version'
