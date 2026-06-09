# Runtime Assets

This directory is reserved for locally managed service artifacts that are shipped,
downloaded, or linked beside the native Qt application.

Expected layout:

```text
runtime/
  bitcoind/
  electrs/
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

The current scaffold starts `bitcoind`, `electrs`, and `node` from PATH by
default. Service-specific executable paths can later be exposed in the settings
screen or set through `ConfigManager`.
