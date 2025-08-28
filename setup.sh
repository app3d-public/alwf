#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

# --- Submodules ---
SUBMODULES=(
  "cmake"
  "modules/acul"
  "modules/awin"
)

echo "[alwf] Syncing submodules..."
git submodule sync -- "${SUBMODULES[@]}"

for path in "${SUBMODULES[@]}"; do
  echo "[alwf] Updating $path"
  git submodule update --init --checkout --depth 1 -- "$path"
done

# --- Node.js deps ---
PUG_DIR="$ROOT_DIR/src/pug-compile"

if command -v node >/dev/null 2>&1 && command -v npm >/dev/null 2>&1; then
  echo "[alwf] Installing npm deps in $PUG_DIR"
  (cd "$PUG_DIR" && npm ci)
else
  echo "[alwf] Node.js or npm not found! Please install them first."
  exit 1
fi

echo "[alwf] Setup complete."
