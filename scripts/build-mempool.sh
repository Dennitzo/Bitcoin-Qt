#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/runtime-src"
PREFIX="$ROOT_DIR/runtime/mempool"
NODE="$ROOT_DIR/runtime/node/bin/node"
NPM="$ROOT_DIR/runtime/node/bin/npm"
REF="${MEMPOOL_REF:-master}"

if [[ ! -x "$NODE" || ! -x "$NPM" ]]; then
  "$ROOT_DIR/scripts/build-node-runtime.sh"
fi

mkdir -p "$BUILD_DIR" "$PREFIX"

if [[ ! -d "$BUILD_DIR/mempool/.git" ]]; then
  git clone https://github.com/mempool/mempool.git "$BUILD_DIR/mempool"
fi

cd "$BUILD_DIR/mempool"
git fetch --tags --prune
git checkout "$REF"

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*)
    if [[ -z "${CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER:-}" ]]; then
      for candidate in \
        "/c/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC"/*/bin/Hostx64/x64/link.exe \
        "/c/Program Files/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC"/*/bin/Hostx64/x64/link.exe \
        "/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC"/*/bin/Hostx64/x64/link.exe; do
        if [[ -x "$candidate" ]]; then
          export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER="$candidate"
          break
        fi
      done
    fi
    if [[ -z "${CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER:-}" ]]; then
      echo "MSVC link.exe not found. Ensure the Visual Studio C++ build tools are available." >&2
      exit 1
    fi
    "$NODE" <<'JS'
const fs = require('fs');
const path = 'rust/gbt/package.json';
const pkg = JSON.parse(fs.readFileSync(path, 'utf8'));
pkg.scripts['check-cargo-version'] = 'cargo version';
pkg.scripts['build-release'] = 'npm run build -- --release';
pkg.scripts['to-backend'] = 'node copy-to-backend.js';
fs.writeFileSync(path, `${JSON.stringify(pkg, null, 2)}\n`);
fs.writeFileSync('rust/gbt/copy-to-backend.js', `
const fs = require('fs');
const path = require('path');

const dest = path.resolve(__dirname, '../../backend/rust-gbt');
fs.rmSync(dest, {recursive: true, force: true});
fs.mkdirSync(dest, {recursive: true});

for (const name of ['index.js', 'index.d.ts', 'package.json']) {
  fs.copyFileSync(path.join(__dirname, name), path.join(dest, name));
}

for (const name of fs.readdirSync(__dirname)) {
  if (name.endsWith('.node')) {
    fs.copyFileSync(path.join(__dirname, name), path.join(dest, name));
  }
}
`);

for (const path of ['backend/package.json', 'frontend/package.json']) {
  const packageJson = JSON.parse(fs.readFileSync(path, 'utf8'));
  for (const [name, script] of Object.entries(packageJson.scripts || {})) {
    packageJson.scripts[name] = script
      .replaceAll('./node_modules/typescript/bin/tsc', 'node ./node_modules/typescript/bin/tsc')
      .replaceAll('./node_modules/@angular/cli/bin/ng.js', 'node ./node_modules/@angular/cli/bin/ng.js')
      .replaceAll('./node_modules/.bin/eslint', 'eslint')
      .replaceAll('./node_modules/.bin/jest', 'jest');
  }
  if (path === 'frontend/package.json') {
    packageJson.scripts['sync-assets'] = 'npm run copy-themes && node copy-resources.js';
  }
  fs.writeFileSync(path, `${JSON.stringify(packageJson, null, 2)}\n`);
}

fs.writeFileSync('frontend/copy-resources.js', `
const fs = require('fs');
const path = require('path');

const source = path.resolve(__dirname, 'src/resources');
const dest = path.resolve(__dirname, 'dist/mempool/browser/resources');
fs.rmSync(dest, {recursive: true, force: true});
fs.mkdirSync(path.dirname(dest), {recursive: true});
fs.cpSync(source, dest, {recursive: true});
`);
JS
    ;;
esac

for package_dir in backend frontend; do
  cd "$BUILD_DIR/mempool/$package_dir"
  if [[ -f package-lock.json ]]; then
    "$NPM" ci
  else
    "$NPM" install
  fi
  if [[ "$package_dir" == "frontend" ]]; then
    SKIP_SYNC=1 "$NPM" run build
  else
    "$NPM" run build
  fi
done

rm -rf "$PREFIX"
mkdir -p "$PREFIX/backend" "$PREFIX/frontend"

cp -a "$BUILD_DIR/mempool/backend/." "$PREFIX/backend/"
rm -rf "$PREFIX/backend/.git" "$PREFIX/backend/test" "$PREFIX/backend/tests"

mkdir -p "$PREFIX/frontend/dist"
cp -a "$BUILD_DIR/mempool/frontend/dist/." "$PREFIX/frontend/dist/"

cat > "$PREFIX/backend/server.js" <<'JS'
process.chdir(__dirname);
process.argv = [process.argv[0], require.resolve('./dist/index.js')];
require('./dist/index.js');
JS

cp "$ROOT_DIR/scripts/mempool-frontend-server.js" "$PREFIX/frontend/server.js"
chmod +x "$PREFIX/frontend/server.js"

test -f "$PREFIX/backend/dist/index.js"
test -f "$PREFIX/frontend/server.js"
echo "Mempool staged at $PREFIX"
