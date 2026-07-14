param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$QtRoot = 'C:\Qt',
    [string]$QtKitPath,
    [string]$CMakePath = 'C:\Program Files\CMake\bin\cmake.exe',
    [switch]$StageRuntime,
    [switch]$BuildElectrs,
    [switch]$Clean
)

. (Join-Path $PSScriptRoot 'common.ps1')

$root = Get-ProjectRoot

if ($StageRuntime) {
    $stageRuntime = Join-Path $PSScriptRoot 'stage-runtime.ps1'
    & $stageRuntime -BuildElectrs:$BuildElectrs
}

$cmake = Resolve-CMakeExecutable -CMakePath $CMakePath
$qtKit = Resolve-QtKit -QtRoot $QtRoot -QtKitPath $QtKitPath
Initialize-BuildEnvironment -QtKit $qtKit

$ninja = Resolve-NinjaExecutable
if (!$ninja) {
    throw "ninja.exe not found. Install Ninja or the Qt Tools/Ninja component."
}
Add-PathEntry (Split-Path -Parent $ninja)

$buildDirName = "windows-$($qtKit.Version)-$($qtKit.Name)-$Configuration"
$buildDir = Join-Path $root "build\$buildDirName"
if ($Clean) {
    Remove-SafeDirectory $buildDir
}
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Write-Step "Configuring Bitcoin-Qt with Qt $($qtKit.Version) $($qtKit.Name)"
Invoke-Tool $cmake `
    '-S' $root `
    '-B' $buildDir `
    '-G' 'Ninja' `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DCMAKE_PREFIX_PATH=$($qtKit.Path)" `
    "-DCMAKE_MAKE_PROGRAM=$ninja"

Write-Step "Building Bitcoin-Qt ($Configuration)"
Invoke-Tool $cmake '--build' $buildDir '--config' $Configuration '--parallel'

$exe = Get-ChildItem -LiteralPath $buildDir -Recurse -Filter 'Bitcoin-Qt.exe' -ErrorAction SilentlyContinue |
    Sort-Object FullName |
    Select-Object -First 1
if ($exe) {
    $runtimeSource = Join-Path $root 'windows\runtime'
    if (!(Test-Path -LiteralPath $runtimeSource)) {
        $runtimeSource = Join-Path $root 'runtime'
    }
    if (!(Test-Path -LiteralPath $runtimeSource)) {
        throw "Runtime source directory not found below $root"
    }
    $runtimeDestination = Join-Path $exe.DirectoryName 'runtime'
    Write-Step "Synchronizing runtime to $runtimeDestination"
    Invoke-Tool $cmake '-E' 'copy_directory' $runtimeSource $runtimeDestination
    Write-Host "Windows executable: $($exe.FullName)"
} else {
    Write-Warning "Build completed but Bitcoin-Qt.exe was not found below $buildDir"
}
