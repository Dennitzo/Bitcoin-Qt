# Bitcoin Node Desktop

Native Qt 6 desktop application for managing a local Bitcoin full node stack:

- Bitcoin Core (`bitcoind`)
- electrs
- Mempool backend
- Mempool frontend embedded with Qt WebEngine / Chromium

The app asks for a storage location on first start. Choose an external drive if
the full node data should live outside the system disk.

## Build

Required Qt modules:

- Qt Widgets
- Qt Network
- Qt Concurrent
- Qt WebEngine
- Qt WebChannel
- Qt Positioning

```bash
/Applications/CMake.app/Contents/bin/cmake -S . -B build/qt-node-desktop \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos"

/Applications/CMake.app/Contents/bin/cmake --build build/qt-node-desktop
```

If `cmake` and Qt are both in your shell environment, the shorter form works:

```bash
cmake -S . -B build/qt-node-desktop -DCMAKE_BUILD_TYPE=Debug
cmake --build build/qt-node-desktop
```

## Runtime

The app owns its service runtime. It resolves binaries from `runtime/` during
development and from `BitcoinNodeDesktop.app/Contents/Resources/runtime` when
packaged.

Expected runtime layout:

```text
runtime/
  bitcoin/bin/bitcoind
  electrs/bin/electrs
  node/bin/node
  mempool/backend/
  mempool/frontend/
```

Build all runtime components:

```bash
./scripts/build-runtime.sh
```

Build individual components:

```bash
./scripts/build-bitcoin-core.sh
./scripts/build-electrs.sh
./scripts/build-node-runtime.sh
./scripts/build-mempool.sh
```

Version pins can be overridden:

```bash
BITCOIN_CORE_REF=v29.0 ELECTRS_REF=master MEMPOOL_REF=master NODE_VERSION=v24.13.0 \
  ./scripts/build-runtime.sh
```

## macOS 12 Notes

Qt 6.11 requires macOS 13 or newer. On macOS 12.7.x, install a complete Qt
6.8.x or 6.9.x macOS kit with these add-ons selected:

- Qt WebEngine
- Qt WebChannel
- Qt Positioning

The Qt Maintenance Tool can fail with a `QmakeOutputInstallerKey` error when it
tries to install Qt 6.11 add-ons on macOS 12, because that kit's `qmake` cannot
run on the operating system. Use a macOS-12-compatible Qt kit instead.
