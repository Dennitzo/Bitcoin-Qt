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

```bash
/Applications/CMake.app/Contents/bin/cmake -S . -B build/qt-node-desktop \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.1/macos"

/Applications/CMake.app/Contents/bin/cmake --build build/qt-node-desktop
```

If `cmake` and Qt are both in your shell environment, the shorter form works:

```bash
cmake -S . -B build/qt-node-desktop -DCMAKE_BUILD_TYPE=Debug
cmake --build build/qt-node-desktop
```

## Runtime

The initial scaffold starts `bitcoind`, `electrs`, and `node` from `PATH`.
Runtime packaging and download management are reserved for `runtime/`.
