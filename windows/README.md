# Native Windows Build

This folder contains PowerShell scripts for a native Windows x64 setup. They do
not require WSL, Git Bash, or MSYS.

Defaults:

- Qt root: `C:\Qt`
- CMake: `C:\Program Files\CMake\bin\cmake.exe`
- Bitcoin Core: latest version detected from `https://bitcoincore.org/en/download/`
- Node.js: `v24.13.0`, override with `NODE_VERSION`
- MariaDB: `11.4.12` portable LTS zip, override with `MARIADB_VERSION`
- Public Pool: built from upstream `public-pool` and `public-pool-ui` with the
  staged Node.js runtime

Check the local toolchain:

```powershell
.\windows\check-environment.ps1
```

Stage prebuilt runtime components into `windows\runtime`:

```powershell
.\windows\stage-runtime.ps1
```

This downloads and stages:

- `windows\runtime\bitcoin\bin\bitcoind.exe`
- `windows\runtime\bitcoin\bin\bitcoin-cli.exe`
- `windows\runtime\node\bin\node.exe`
- `windows\runtime\node\bin\npm.cmd`
- `windows\runtime\mariadb\bin\mariadbd.exe`
- `windows\runtime\mariadb\bin\mariadb-install-db.exe`
- `windows\runtime\mempool\backend\server.js`
- `windows\runtime\mempool\frontend\server.js`
- `windows\runtime\public-pool\backend\dist\main.js`
- `windows\runtime\public-pool\frontend\server.js`

The Mempool build requires Git for Windows, Rust with the
`x86_64-pc-windows-msvc` toolchain, and Visual Studio C++ build tools. To build
only this runtime component, run:

```powershell
.\windows\stage-node-runtime.ps1
.\windows\stage-mempool.ps1
```

Set `MEMPOOL_REF` or pass `-Ref` to build a branch, tag, or commit other than
`master`. Pass `-ForceUpdate` to fetch updated refs in an existing checkout.

To refresh the Public Pool Git checkouts before building, run:

```powershell
.\windows\stage-public-pool.ps1 -ForceUpdate
```

`electrs` does not publish an official Windows x64 binary in the same way. To
build it natively, install Git for Windows, Rust, LLVM, and Visual Studio C++
tools, then run:

```powershell
.\windows\stage-electrs.ps1
```

Build the Qt app:

```powershell
.\windows\build-app.ps1 -Configuration Release
```

To stage the prebuilt runtime first and then build:

```powershell
.\windows\build-app.ps1 -StageRuntime -Configuration Release
```

If multiple Qt kits exist, pass one explicitly:

```powershell
.\windows\build-app.ps1 -QtKitPath C:\Qt\6.11.1\msvc2022_64 -Configuration Release
```

The build requires one complete x64 Qt kit with these modules installed in the
same kit:

- Qt6
- Qt6Concurrent
- Qt6Network
- Qt6WebChannel
- Qt6WebEngineWidgets
- Qt6Widgets

The script prefers MSVC x64 kits because Qt WebEngine is commonly installed
there. If only MinGW base Qt is installed without Qt WebEngine, install the
matching Qt WebEngine add-on or an MSVC x64 kit that includes both Qt base and
Qt WebEngine.
