param(
    [string]$QtRoot = 'C:\Qt',
    [string]$CMakePath = 'C:\Program Files\CMake\bin\cmake.exe'
)

. (Join-Path $PSScriptRoot 'common.ps1')

Write-Step "Checking native Windows build environment"

$cmake = Resolve-CMakeExecutable -CMakePath $CMakePath
Write-Host "CMake: $cmake"
& $cmake --version | Select-Object -First 1

Write-Host ''
Write-Host "Qt kits below $QtRoot"
$kits = @(Get-QtKits -QtRoot $QtRoot)
foreach ($kit in $kits) {
    $missing = if ($kit.MissingModules.Count -eq 0) { 'none' } else { $kit.MissingModules -join ', ' }
    $deploy = if ($kit.WindeployQt) { $kit.WindeployQt } else { 'missing' }
    Write-Host "  $($kit.Version)\$($kit.Name)"
    Write-Host "    path: $($kit.Path)"
    Write-Host "    missing modules: $missing"
    Write-Host "    windeployqt: $deploy"
}

Write-Host ''
try {
    $selected = Resolve-QtKit -QtRoot $QtRoot
    Write-Host "Selected Qt kit: $($selected.Path)"
} catch {
    Write-Warning $_.Exception.Message
}

Write-Host ''
Write-Host "Latest Bitcoin Core according to bitcoincore.org: $(Get-LatestBitcoinCoreVersion)"
