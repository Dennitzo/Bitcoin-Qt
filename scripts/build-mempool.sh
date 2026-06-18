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

"$NODE" <<'JS'
const fs = require('fs');
const difficultyPath = 'frontend/src/app/components/difficulty/difficulty.component.ts';
let difficulty = fs.readFileSync(difficultyPath, 'utf8');
if (!difficulty.includes('if (start > end)')) {
  difficulty = difficulty.replace(
`    if (startX > endX) {
      return [];
    }`,
`    if (start > end || startX > endX) {
      return [];
    }`);
  fs.writeFileSync(difficultyPath, difficulty);
}

const hashrateChartPath = 'frontend/src/app/components/hashrate-chart/hashrate-chart.component.ts';
let hashrateChart = fs.readFileSync(hashrateChartPath, 'utf8');
hashrateChart = hashrateChart.replace(
  "text: $localize`:@@23555386d8af1ff73f297e89dd4af3f4689fb9dd:Indexing blocks`,",
  "text: $localize`:@@23555386d8af1ff73f297e89dd4af3f4689fb9dd:Indexing network hashrate`,"
);
fs.writeFileSync(hashrateChartPath, hashrateChart);

const hashratesRepositoryPath = 'backend/src/repositories/HashratesRepository.ts';
let hashratesRepository = fs.readFileSync(hashratesRepositoryPath, 'utf8');
if (!hashratesRepository.includes('NO_AUTO_VALUE_ON_ZERO')) {
  hashratesRepository = hashratesRepository.replace(
`    let query = \`INSERT INTO
      hashrates(hashrate_timestamp, avg_hashrate, pool_id, share, type) VALUES\`;`,
`    if (hashrates.some((hashrate) => hashrate.poolId === 0)) {
      await DB.query(\`SET SESSION sql_mode = IF(
        FIND_IN_SET('NO_AUTO_VALUE_ON_ZERO', @@SESSION.sql_mode),
        @@SESSION.sql_mode,
        CONCAT_WS(',', @@SESSION.sql_mode, 'NO_AUTO_VALUE_ON_ZERO')
      )\`);
      await DB.query(\`
        INSERT IGNORE INTO pools(id, name, link, addresses, regexes, slug, unique_id)
        VALUES (0, 'Network', '', '[]', '[]', 'network', -2)
      \`);
    }

    let query = \`INSERT INTO
      hashrates(hashrate_timestamp, avg_hashrate, pool_id, share, type) VALUES\`;`
  );
  fs.writeFileSync(hashratesRepositoryPath, hashratesRepository);
}
JS

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

"$NODE" <<'JS'
const fs = require('fs');
const migrationPath = 'backend/dist/api/database-migration.js';
if (!fs.existsSync(migrationPath)) {
  console.log(`${migrationPath} not found, skipping compatibility migration patch`);
  process.exit(0);
}
let migration = fs.readFileSync(migrationPath, 'utf8');
migration = migration.replaceAll(
  "ALTER TABLE blocks DROP FOREIGN KEY IF EXISTS `blocks_ibfk_1`",
  "ALTER TABLE blocks DROP FOREIGN KEY IF EXISTS `blocks_ibfk_1`');\\n      await this.$executeQuery('ALTER TABLE blocks DROP FOREIGN KEY IF EXISTS `1`"
);
migration = migration.replaceAll(
  "ALTER TABLE `hashrates` DROP FOREIGN KEY `hashrates_ibfk_1`",
  "ALTER TABLE `hashrates` DROP FOREIGN KEY IF EXISTS `hashrates_ibfk_1`"
);
migration = migration.replaceAll(
  "await this.$executeQuery('START TRANSACTION;');",
  "await this.$executeQuery('SET FOREIGN_KEY_CHECKS=0;');\\n            await this.$executeQuery('START TRANSACTION;')"
);
migration = migration.replaceAll(
  "await this.$executeQuery('COMMIT;');",
  "await this.$executeQuery('COMMIT;');\\n            await this.$executeQuery('SET FOREIGN_KEY_CHECKS=1;')"
);
migration = migration.replaceAll(
  "await this.$executeQuery('ROLLBACK;');",
  "await this.$executeQuery('ROLLBACK;');\\n            await this.$executeQuery('SET FOREIGN_KEY_CHECKS=1;')"
);
migration = migration.replaceAll(
  "await this.$createMissingTablesAndIndexes(databaseSchemaVersion);",
  "await this.$executeQuery('SET FOREIGN_KEY_CHECKS=0;');\\n            await this.$createMissingTablesAndIndexes(databaseSchemaVersion);\\n            await this.$executeQuery('SET FOREIGN_KEY_CHECKS=1;')"
);
fs.writeFileSync(migrationPath, migration);
JS

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
# Added fallback for npm ci failure
if [[ "$package_dir" == "backend" || "$package_dir" == "frontend" ]]; then
  cd "$BUILD_DIR/mempool/$package_dir"
  if [[ -f package-lock.json ]]; then
    # Try ci, fall back to install on error
    if ! "$NPM" ci; then
      echo "npm ci failed, falling back to npm install for $package_dir" >&2
      "$NPM" install
    fi
  else
    "$NPM" install
  fi
  if [[ "$package_dir" == "frontend" ]]; then
    SKIP_SYNC=1 "$NPM" run build
  else
    "$NPM" run build
  fi
fi