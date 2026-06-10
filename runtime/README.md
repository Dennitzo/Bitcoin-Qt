# Runtime Assets

This directory contains locally managed service artifacts that are shipped beside
the native Qt application. The desktop app resolves services from this directory
instead of relying on binaries from the user's shell `PATH`.

Expected layout:

```text
runtime/
  bitcoin/
    bin/
      bitcoind
  electrs/
    bin/
      electrs
  node/
    bin/
      node
  mempool/
    backend/
    frontend/
```

The selected first-run storage directory is separate from this folder. It holds
large mutable node data on an internal or external disk:

```text
<selected-disk>/
  bitcoin/
  electrs/
  mempool/
  logs/
```

Build or stage the runtime with:

```bash
./scripts/build-runtime.sh
```

Individual services can be built with:

```bash
./scripts/build-bitcoin-core.sh
./scripts/build-electrs.sh
./scripts/build-node-runtime.sh
./scripts/build-mempool.sh
```

For packaged `.app` bundles, copy this `runtime/` directory into
`BitcoinNodeDesktop.app/Contents/Resources/runtime`.
