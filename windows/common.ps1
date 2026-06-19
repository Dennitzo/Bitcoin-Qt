Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:WindowsDir = Split-Path -Parent $PSCommandPath
$script:ProjectRoot = (Resolve-Path (Join-Path $script:WindowsDir '..')).Path

function Get-ProjectRoot {
    return $script:ProjectRoot
}

function Resolve-RuntimeRoot {
    param([string]$RuntimeRoot = $env:BITCOIN_QT_RUNTIME_ROOT)

    if ([string]::IsNullOrWhiteSpace($RuntimeRoot)) {
        return (Join-Path $script:ProjectRoot 'windows\runtime')
    }

    if ([System.IO.Path]::IsPathRooted($RuntimeRoot)) {
        return $RuntimeRoot
    }

    return (Join-Path $script:ProjectRoot $RuntimeRoot)
}

function Write-Step {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host "==> $Message"
}

function Add-PathEntry {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (!(Test-Path -LiteralPath $Path)) {
        return
    }
    $entries = $env:PATH -split ';'
    if ($entries -notcontains $Path) {
        $env:PATH = "$Path;$env:PATH"
    }
}

function Resolve-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [string[]]$Candidates = @()
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

function Resolve-GitExecutable {
    $git = Resolve-Tool -Name 'git.exe' -Candidates @(
        'C:\Program Files\Git\cmd\git.exe',
        'C:\Program Files\Git\bin\git.exe'
    )
    if ($git) {
        return $git
    }

    $candidate = Get-ChildItem -LiteralPath 'C:\Program Files' -Recurse -Filter git.exe -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -like '*\Git\cmd\git.exe' -or $_.FullName -like '*\Git\bin\git.exe' -or $_.FullName -like '*\Git\mingw64\bin\git.exe' } |
        Select-Object -First 1
    if ($candidate) {
        return $candidate.FullName
    }

    return $null
}

function Invoke-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Remove-SafeDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (!(Test-Path -LiteralPath $Path)) {
        return
    }

    $root = [System.IO.Path]::GetFullPath($script:ProjectRoot).TrimEnd('\', '/')
    $target = [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
    if ($target.Equals($root, [System.StringComparison]::OrdinalIgnoreCase) -or
        !$target.StartsWith("$root\", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove directory outside project root: $target"
    }

    Remove-Item -LiteralPath $target -Recurse -Force
}

function Download-File {
    param(
        [Parameter(Mandatory = $true)][string]$Uri,
        [Parameter(Mandatory = $true)][string]$OutFile,
        [switch]$Force
    )

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutFile) | Out-Null
    if ((Test-Path -LiteralPath $OutFile) -and !$Force) {
        Write-Host "Using cached download: $OutFile"
        return
    }

    Write-Host "Downloading $Uri"
    Invoke-WebRequest -Uri $Uri -OutFile $OutFile -UseBasicParsing
}

function Expand-ZipFile {
    param(
        [Parameter(Mandatory = $true)][string]$Archive,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    Remove-SafeDirectory $Destination
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
}

function Get-RequiredQtModules {
    return @(
        'Qt6',
        'Qt6Concurrent',
        'Qt6Network',
        'Qt6Positioning',
        'Qt6WebChannel',
        'Qt6WebEngineWidgets',
        'Qt6Widgets'
    )
}

function Get-MissingQtModules {
    param([Parameter(Mandatory = $true)][string]$KitPath)

    $missing = New-Object System.Collections.Generic.List[string]
    foreach ($module in Get-RequiredQtModules) {
        if ($module -eq 'Qt6') {
            $config = Join-Path $KitPath 'lib\cmake\Qt6\Qt6Config.cmake'
        } else {
            $config = Join-Path $KitPath "lib\cmake\$module\$module`Config.cmake"
        }
        if (!(Test-Path -LiteralPath $config)) {
            $missing.Add($module)
        }
    }
    return $missing.ToArray()
}

function Get-KitPreferenceRank {
    param([Parameter(Mandatory = $true)][string]$KitName)
    if ($KitName -eq 'msvc2022_64') { return 0 }
    if ($KitName -like 'msvc*_64') { return 1 }
    if ($KitName -eq 'llvm-mingw_64') { return 2 }
    if ($KitName -like 'llvm-mingw*_64') { return 3 }
    if ($KitName -eq 'mingw_64') { return 4 }
    if ($KitName -like 'mingw*_64') { return 5 }
    return 6
}

function Get-QtKits {
    param([string]$QtRoot = 'C:\Qt')

    if (!(Test-Path -LiteralPath $QtRoot)) {
        throw "Qt root not found: $QtRoot"
    }

    $kits = New-Object System.Collections.Generic.List[object]
    $versions = Get-ChildItem -LiteralPath $QtRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+(\.\d+)?$' } |
        Sort-Object @{ Expression = { [version]$_.Name }; Descending = $true }

    foreach ($versionDir in $versions) {
        $kitDirs = Get-ChildItem -LiteralPath $versionDir.FullName -Directory |
            Where-Object { $_.Name -match '^(msvc|mingw|llvm-mingw).*(x64|_64)$' }

        foreach ($kitDir in $kitDirs) {
            $missing = @(Get-MissingQtModules -KitPath $kitDir.FullName)
            $windeployqt = Find-WindeployQt -KitPath $kitDir.FullName
            $kits.Add([pscustomobject]@{
                Version = $versionDir.Name
                Name = $kitDir.Name
                Path = $kitDir.FullName
                Toolchain = if ($kitDir.Name -like '*mingw*') { 'mingw' } else { 'msvc' }
                MissingModules = $missing
                IsComplete = $missing.Count -eq 0
                WindeployQt = $windeployqt
            })
        }
    }

    return $kits |
        Sort-Object @{ Expression = { [version]$_.Version }; Descending = $true },
                    @{ Expression = { Get-KitPreferenceRank $_.Name }; Descending = $false }
}

function Find-WindeployQt {
    param([Parameter(Mandatory = $true)][string]$KitPath)

    foreach ($name in @('windeployqt.exe', 'windeployqt6.exe')) {
        $candidate = Join-Path $KitPath "bin\$name"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return $null
}

function Resolve-QtKit {
    param(
        [string]$QtRoot = 'C:\Qt',
        [string]$QtKitPath
    )

    if ($QtKitPath) {
        $resolved = (Resolve-Path -LiteralPath $QtKitPath).Path
        $missing = @(Get-MissingQtModules -KitPath $resolved)
        if ($missing.Count -gt 0) {
            throw "Qt kit is missing required modules ($($missing -join ', ')): $resolved"
        }
        $kitName = Split-Path -Leaf $resolved
        $version = Split-Path -Leaf (Split-Path -Parent $resolved)
        return [pscustomobject]@{
            Version = $version
            Name = $kitName
            Path = $resolved
            Toolchain = if ($kitName -like '*mingw*') { 'mingw' } else { 'msvc' }
            MissingModules = @()
            IsComplete = $true
            WindeployQt = Find-WindeployQt -KitPath $resolved
        }
    }

    $kits = @(Get-QtKits -QtRoot $QtRoot)
    $complete = $kits | Where-Object { $_.IsComplete } | Select-Object -First 1
    if ($complete) {
        return $complete
    }

    $report = $kits | ForEach-Object {
        $missingText = if ($_.MissingModules.Count -eq 0) { 'none' } else { $_.MissingModules -join ', ' }
        "  $($_.Version)\$($_.Name): missing $missingText"
    }
    throw "No complete Qt x64 kit found below $QtRoot. Required modules: $((Get-RequiredQtModules) -join ', ').`n$($report -join "`n")"
}

function Resolve-CMakeExecutable {
    param([string]$CMakePath = 'C:\Program Files\CMake\bin\cmake.exe')

    if ($CMakePath -and (Test-Path -LiteralPath $CMakePath)) {
        return $CMakePath
    }

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $qtCMake = Get-ChildItem -LiteralPath 'C:\Qt\Tools' -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($qtCMake) {
        return $qtCMake.FullName
    }

    throw "CMake not found. Expected $CMakePath"
}

function Resolve-NinjaExecutable {
    $candidate = Resolve-Tool -Name 'ninja.exe' -Candidates @('C:\Qt\Tools\Ninja\ninja.exe')
    return $candidate
}

function Import-VisualStudioEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }

    $vsDevCmd = $null
    $vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vsWhere) {
        $installPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installPath) {
            $candidate = Join-Path $installPath 'Common7\Tools\VsDevCmd.bat'
            if (Test-Path -LiteralPath $candidate) {
                $vsDevCmd = $candidate
            }
        }
    }

    if (!$vsDevCmd) {
        $vsDevCmd = Get-ChildItem -LiteralPath 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter VsDevCmd.bat -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
    }

    if (!$vsDevCmd) {
        throw "Visual Studio C++ environment not found. Install the Desktop development with C++ workload."
    }

    Write-Step "Importing Visual Studio environment from $vsDevCmd"
    $environment = & cmd.exe /s /c "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && set"
    foreach ($line in $environment) {
        $separator = $line.IndexOf('=')
        if ($separator -le 0) {
            continue
        }
        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        Set-Item -Path "Env:$name" -Value $value
    }
}

function Initialize-BuildEnvironment {
    param([Parameter(Mandatory = $true)]$QtKit)

    Add-PathEntry (Join-Path $QtKit.Path 'bin')

    if ($QtKit.Toolchain -eq 'mingw') {
        $toolFilter = if ($QtKit.Name -like 'llvm-mingw*') { 'llvm-mingw*_64' } else { 'mingw*_64' }
        $mingw = Get-ChildItem -LiteralPath 'C:\Qt\Tools' -Directory -Filter $toolFilter -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            Select-Object -First 1
        if ($mingw) {
            Add-PathEntry (Join-Path $mingw.FullName 'bin')
        }
        Add-PathEntry 'C:\Qt\Tools\Ninja'
        return
    }

    Import-VisualStudioEnvironment
}

function Get-LatestBitcoinCoreVersion {
    param([string]$FallbackVersion = '31.0')

    try {
        $downloadPage = Invoke-WebRequest -Uri 'https://bitcoincore.org/en/download/' -UseBasicParsing -TimeoutSec 30
        if ($downloadPage.Content -match 'Latest version:\s*([0-9]+(?:\.[0-9]+)*)') {
            return $Matches[1]
        }
    } catch {
        Write-Warning "Could not fetch latest Bitcoin Core version: $($_.Exception.Message)"
    }

    return $FallbackVersion
}
