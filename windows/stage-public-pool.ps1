param(
    [string]$RuntimeRoot = $env:BITCOIN_QT_RUNTIME_ROOT,
    [switch]$ForceUpdate
)

. (Join-Path $PSScriptRoot 'common.ps1')

$root = Get-ProjectRoot
$runtimeRoot = Resolve-RuntimeRoot -RuntimeRoot $RuntimeRoot
$buildDir = Join-Path $root 'build\runtime-src'
$backendSrc = Join-Path $buildDir 'public-pool'
$frontendSrc = Join-Path $buildDir 'public-pool-ui'
$prefix = Join-Path $runtimeRoot 'public-pool'
$node = Join-Path $runtimeRoot 'node\bin\node.exe'
$npm = Join-Path $runtimeRoot 'node\bin\npm.cmd'
$git = Resolve-GitExecutable

Write-Step "Staging Public Pool for native Windows runtime"

if (!(Test-Path -LiteralPath $node) -or !(Test-Path -LiteralPath $npm)) {
    throw "Node runtime missing below $runtimeRoot. Run windows\stage-node-runtime.ps1 first."
}
if (!$git) {
    throw "Git for Windows was not found. Install Git, or make git.exe available on PATH."
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

function Sync-GitRepository {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$Path
    )

    if (!(Test-Path -LiteralPath (Join-Path $Path '.git'))) {
        Invoke-Tool $git 'clone' $Url $Path
        return
    }

    if ($ForceUpdate) {
        Invoke-Tool $git '-C' $Path 'pull' '--ff-only' '--autostash'
    } else {
        Write-Host "Using existing checkout: $Path"
    }
}

Sync-GitRepository -Url 'https://github.com/benjamin-wilson/public-pool.git' -Path $backendSrc
Sync-GitRepository -Url 'https://github.com/benjamin-wilson/public-pool-ui.git' -Path $frontendSrc

function Invoke-SharedPublicPoolPatches {
    $unixBuildScript = Join-Path $root 'scripts\build-public-pool.sh'
    $lines = Get-Content -LiteralPath $unixBuildScript
    $patchCount = 0

    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index] -notmatch '^(BACKEND_SRC|FRONTEND_SRC)=.*<<''JS''$') {
            continue
        }

        $variable = $Matches[1]
        $scriptLines = New-Object System.Collections.Generic.List[string]
        for ($index++; $index -lt $lines.Count -and $lines[$index] -ne 'JS'; $index++) {
            $scriptLines.Add($lines[$index])
        }
        if ($index -ge $lines.Count) {
            throw "Unterminated JavaScript patch block in $unixBuildScript"
        }

        $patchCount++
        $scriptPath = Join-Path $buildDir "public-pool-patch-$patchCount.js"
        Set-Content -LiteralPath $scriptPath -Value ($scriptLines -join "`n") -Encoding UTF8
        $previousValue = [Environment]::GetEnvironmentVariable($variable, 'Process')
        [Environment]::SetEnvironmentVariable(
            $variable,
            $(if ($variable -eq 'BACKEND_SRC') { $backendSrc } else { $frontendSrc }),
            'Process'
        )
        try {
            Invoke-Tool $node $scriptPath
        } finally {
            [Environment]::SetEnvironmentVariable($variable, $previousValue, 'Process')
            Remove-Item -LiteralPath $scriptPath -Force -ErrorAction SilentlyContinue
        }
    }

    if ($patchCount -ne 3) {
        throw "Expected 3 shared Public Pool patch blocks, found $patchCount in $unixBuildScript"
    }
}

Write-Step "Applying shared Public Pool backend and frontend patches"
Invoke-SharedPublicPoolPatches

Write-Step "Building Public Pool backend"
Push-Location $backendSrc
try {
    Invoke-Tool $npm 'ci'
    Invoke-Tool $npm 'run' 'build'
    Invoke-Tool $npm 'prune' '--omit=dev'
} finally {
    Pop-Location
}

Write-Step "Building Public Pool frontend"
$environmentFile = Join-Path $frontendSrc 'src\environments\environment.electron.ts'
if (Test-Path -LiteralPath $environmentFile) {
    $environmentSource = Get-Content -LiteralPath $environmentFile -Raw
    if ($environmentSource -notmatch 'SECURE_STRATUM_URL') {
        $environmentSource = $environmentSource.Replace(
            "STRATUM_URL: 'localhost:3333'",
            "STRATUM_URL: 'localhost:3333',`r`n    SECURE_STRATUM_URL: 'localhost:4333'"
        )
        Set-Content -LiteralPath $environmentFile -Value $environmentSource -Encoding UTF8
    }
}

Push-Location $frontendSrc
try {
    Invoke-Tool $npm 'ci'
    Invoke-Tool $npm 'run' 'build:electron'
} finally {
    Pop-Location
}

Remove-SafeDirectory (Join-Path $prefix 'backend')
Remove-SafeDirectory (Join-Path $prefix 'frontend')
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'backend') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'frontend') | Out-Null

Write-Step "Copying Public Pool runtime files"
Copy-Item -Path (Join-Path $backendSrc '*') -Destination (Join-Path $prefix 'backend') -Recurse -Force
foreach ($relative in @('.git', 'coverage', 'test')) {
    Remove-SafeDirectory (Join-Path $prefix "backend\$relative")
}

$backendMigrationDir = Join-Path $prefix 'backend\dist\api'
New-Item -ItemType Directory -Force -Path $backendMigrationDir | Out-Null
Set-Content -LiteralPath (Join-Path $backendMigrationDir 'database-migration.js') -Encoding ASCII -Value @'
// Compatibility stub for older asset patch steps.
// Current Public Pool builds use TypeORM synchronize and no longer ship this file.
'@

$frontendDist = Join-Path $frontendSrc 'dist\public-pool-ui'
if (!(Test-Path -LiteralPath $frontendDist)) {
    throw "Public Pool UI build output not found: $frontendDist"
}
Copy-Item -LiteralPath $frontendDist -Destination (Join-Path $prefix 'frontend\dist') -Recurse -Force

$chartHelpers = Join-Path $frontendSrc 'node_modules\chart.js\dist\chunks\helpers.segment.js'
$kurkleColor = Join-Path $frontendSrc 'node_modules\@kurkle\color\dist\color.esm.js'
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'frontend\dist\chunks') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'frontend\dist\vendor\@kurkle\color') | Out-Null
if (Test-Path -LiteralPath $chartHelpers) {
    Copy-Item -LiteralPath $chartHelpers -Destination (Join-Path $prefix 'frontend\dist\chunks\helpers.segment.js') -Force
}
if (Test-Path -LiteralPath $kurkleColor) {
    Copy-Item -LiteralPath $kurkleColor -Destination (Join-Path $prefix 'frontend\dist\vendor\@kurkle\color\color.esm.js') -Force
}

Set-Content -LiteralPath (Join-Path $prefix 'frontend\server.js') -Encoding ASCII -Value @'
const http = require('http');
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, 'dist');
const port = Number(process.env.PUBLIC_POOL_FRONTEND_PORT || process.env.PORT || 3335);
const apiPort = Number(process.env.PUBLIC_POOL_API_PORT || 3334);
const stratumPort = Number(process.env.PUBLIC_POOL_STRATUM_PORT || 3333);

const types = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.woff2': 'font/woff2',
};

function sendFile(res, file) {
  fs.readFile(file, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    if (path.basename(file) === 'index.html') {
      const importMap = '<script type="importmap">{"imports":{"@kurkle/color":"./vendor/@kurkle/color/color.esm.js"}}</script>';
      data = Buffer.from(data.toString('utf8')
        .replace('</head>', `${importMap}</head>`)
        .replace(/<script src="(scripts\.[^"]+\.js)" defer><\/script>/, '<script src="$1" type="module"></script>'));
    }
    res.writeHead(200, {'content-type': types[path.extname(file)] || 'application/octet-stream'});
    res.end(data);
  });
}

http.createServer((req, res) => {
  if (req.url === '/bitcoin-qt-health') {
    res.writeHead(200, {'content-type': 'application/json'});
    res.end(JSON.stringify({ok: true, apiPort}));
    return;
  }
  if (req.url === '/assets/runtime-config.js' || req.url === '/runtime-config.js' ||
      req.url === '/assets/bitcoin-qt-config.js' || req.url === '/bitcoin-qt-config.js') {
    res.writeHead(200, {'content-type': 'application/javascript; charset=utf-8'});
    res.end(`window.__PUBLIC_POOL_CONFIG__ = { API_URL: 'http://127.0.0.1:${apiPort}', STRATUM_URL: '127.0.0.1:${stratumPort}', SECURE_STRATUM_URL: '127.0.0.1:4333' };`);
    return;
  }
  const clean = decodeURIComponent((req.url || '/').split('?')[0]).replace(/^\/+/, '');
  const candidate = path.normalize(path.join(root, clean || 'index.html'));
  if (!candidate.startsWith(root)) {
    res.writeHead(403);
    res.end('Forbidden');
    return;
  }
  fs.stat(candidate, (err, stat) => {
    if (!err && stat.isFile()) {
      sendFile(res, candidate);
      return;
    }
    if (path.extname(candidate)) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }
    sendFile(res, path.join(root, 'index.html'));
  });
}).listen(port, '127.0.0.1', () => {
  console.log(`Public Pool UI listening on http://127.0.0.1:${port}`);
});
'@

if (!(Test-Path -LiteralPath (Join-Path $prefix 'backend\dist\main.js'))) {
    throw "Public Pool backend entrypoint missing after staging: $prefix\backend\dist\main.js"
}
if (!(Test-Path -LiteralPath (Join-Path $prefix 'frontend\server.js'))) {
    throw "Public Pool frontend server missing after staging: $prefix\frontend\server.js"
}

Write-Host "Public Pool runtime staged at $prefix"
