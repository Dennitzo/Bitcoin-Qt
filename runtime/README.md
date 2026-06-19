# Runtime Assets

This directory contains locally managed service artifacts that are shipped beside
the native Qt application. The desktop app resolves services from this directory
instead of relying on binaries from the user's shell `PATH`.

Expected layout:

```text
runtime/
  bitcoin/
    bin/
      bitcoind[.exe]
      bitcoin-cli[.exe]
  electrs/
    bin/
      electrs[.exe]
  node/
    bin/
      node[.exe]
      npm[.cmd]
  mariadb/
    bin/
      mariadbd[.exe]
      mariadb-install-db[.exe]
    scripts/
      mariadb-install-db
  mempool/
    backend/
    frontend/
  public-pool/
    backend/
    frontend/
```

The selected first-run storage directory is separate from this folder. It holds
large mutable node data on an internal or external disk:

```text
<selected-disk>/
  bitcoin/
  electrs/
  mempool-db/
  mempool/
  public-pool/
  logs/
```

Build or stage the runtime with:

```bash
./scripts/build-runtime.sh
```

On native Windows without WSL or Git Bash, stage available Windows x64 binaries
into `windows/runtime/` with:

```powershell
.\windows\stage-runtime.ps1
```

The Windows staging script uses portable archives where upstream publishes them:
Bitcoin Core, Node.js, and MariaDB. Build electrs natively with
`.\windows\stage-electrs.ps1` after installing Git for Windows, Rust, LLVM, and
Visual Studio C++ tools.

The runtime directories are generated artifacts and are ignored by git. Rebuild
or restage them locally instead of committing binaries, package-manager output,
compiled frontend bundles, or `node_modules`.

Individual services can be built with:

```bash
./scripts/build-bitcoin-core.sh
./scripts/build-electrs.sh
./scripts/build-node-runtime.sh
./scripts/build-mariadb-runtime-macos.sh
./scripts/build-mempool.sh
./scripts/build-public-pool.sh
```

For packaged `.app` bundles, copy this `runtime/` directory into
`Bitcoin-Qt.app/Contents/Resources/runtime`.
