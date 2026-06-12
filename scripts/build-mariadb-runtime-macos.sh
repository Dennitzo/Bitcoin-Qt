#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="$ROOT_DIR/runtime/mariadb"

copy_runtime() {
  local source_dir="$1"
  if [[ ! -x "$source_dir/bin/mariadbd" && ! -x "$source_dir/bin/mysqld" && ! -x "$source_dir/libexec/mariadbd" && ! -x "$source_dir/libexec/mysqld" ]]; then
    echo "MariaDB server binary not found in $source_dir/bin or $source_dir/libexec" >&2
    exit 1
  fi
  if [[ ! -x "$source_dir/scripts/mariadb-install-db" && ! -x "$source_dir/bin/mariadb-install-db" && ! -x "$source_dir/scripts/mysql_install_db" ]]; then
    echo "MariaDB install-db helper not found in $source_dir/scripts or $source_dir/bin" >&2
    exit 1
  fi

  rm -rf "$PREFIX"
  mkdir -p "$PREFIX"
  rsync -a \
    --exclude 'data' \
    --exclude '*.err' \
    --exclude '*.pid' \
    "$source_dir/" "$PREFIX/"

  chmod +x "$PREFIX/bin/"* 2>/dev/null || true
  chmod +x "$PREFIX/libexec/"* 2>/dev/null || true
  chmod +x "$PREFIX/scripts/"* 2>/dev/null || true
}

if [[ -n "${MARIADB_SOURCE_DIR:-}" ]]; then
  copy_runtime "$MARIADB_SOURCE_DIR"
  echo "MariaDB runtime staged from $MARIADB_SOURCE_DIR"
  exit 0
fi

if command -v brew >/dev/null 2>&1; then
  if ! brew list --versions mariadb >/dev/null 2>&1; then
    brew install mariadb
  fi
  CELLAR_DIR="$(brew --prefix mariadb)"
  copy_runtime "$CELLAR_DIR"
  echo "MariaDB runtime staged from Homebrew: $CELLAR_DIR"
  exit 0
fi

cat >&2 <<'EOF'
No MariaDB source found.

Install Homebrew and rerun this script, or provide a MariaDB installation path:

  MARIADB_SOURCE_DIR=/path/to/mariadb ./scripts/build-mariadb-runtime-macos.sh

Expected source layout:
  bin/mariadbd
  scripts/mariadb-install-db
EOF
exit 1
