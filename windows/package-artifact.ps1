param(
    [string]$ArtifactName = $env:ARTIFACT_NAME,
    [string]$BuildDir,
    [string]$DistDir,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release'
)

. (Join-Path $PSScriptRoot 'common.ps1')

if ([string]::IsNullOrWhiteSpace($ArtifactName)) {
    $ArtifactName = 'bitcoin-qt-windows-x64'
}

$root = Get-ProjectRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $buildRoot = Join-Path $root 'build'
    $buildCandidate = Get-ChildItem -LiteralPath $buildRoot -Directory -Filter "windows-*-msvc2022_64-$Configuration" -ErrorAction SilentlyContinue |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'Bitcoin-Qt.exe') } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (!$buildCandidate) {
        throw "No Windows MSVC build directory with Bitcoin-Qt.exe found below $buildRoot. Pass -BuildDir explicitly."
    }
    $BuildDir = $buildCandidate.FullName
}
if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path $root 'build\dist'
}

$exe = Join-Path $BuildDir 'Bitcoin-Qt.exe'
if (!(Test-Path -LiteralPath $exe)) {
    throw "Built executable not found: $exe"
}

$windowsRuntime = Join-Path $root 'windows\runtime'
if (Test-Path -LiteralPath $windowsRuntime) {
    $buildRuntime = Join-Path $BuildDir 'runtime'
    Remove-SafeDirectory $buildRuntime
    Copy-Item -LiteralPath $windowsRuntime -Destination $buildRuntime -Recurse -Force
}

$stageDir = Join-Path $DistDir $ArtifactName
Remove-SafeDirectory $stageDir
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Write-Step "Staging Windows artifact from $BuildDir"
$itemsToCopy = @(
    '*.exe',
    '*.dll',
    '*.dat',
    '*.pak',
    '*.bin',
    'qt.conf',
    'generic',
    'iconengines',
    'imageformats',
    'networkinformation',
    'platforms',
    'position',
    'qml',
    'qmltooling',
    'resources',
    'runtime',
    'styles',
    'tls',
    'translations'
)

foreach ($item in $itemsToCopy) {
    $matches = Get-ChildItem -LiteralPath $BuildDir -Filter $item -Force -ErrorAction SilentlyContinue
    foreach ($match in $matches) {
        Copy-Item -LiteralPath $match.FullName -Destination $stageDir -Recurse -Force
    }
}

Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $stageDir -Force
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $stageDir -Force

$zipPath = Join-Path $DistDir "$ArtifactName.zip"
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

$sevenZip = Resolve-Tool -Name '7z.exe' -Candidates @(
    'C:\Program Files\7-Zip\7z.exe',
    'C:\ProgramData\chocolatey\bin\7z.exe'
)
if ($sevenZip) {
    Push-Location $DistDir
    try {
        Invoke-Tool $sevenZip 'a' '-tzip' '-mx=5' $zipPath $ArtifactName
    } finally {
        Pop-Location
    }
} else {
    $tar = Resolve-Tool -Name 'tar.exe'
    if ($tar) {
        Invoke-Tool $tar '-a' '-cf' $zipPath '-C' $DistDir $ArtifactName
    } else {
        Compress-Archive -LiteralPath $stageDir -DestinationPath $zipPath -Force
    }
}
Write-Host "Packaged artifact: $zipPath"
