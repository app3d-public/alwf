#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

detect_windows() {
  case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) return 0 ;;
    *) return 1 ;;
  esac
}

echo "==> Init/update: cmake"
git submodule update --init --progress cmake

echo "==> Init/update: modules/acul"
git submodule update --init --progress modules/acul

if detect_windows; then
  echo "==> Init/update: modules/awin (Windows detected)"
  git submodule update --init --progress modules/awin
else
  echo "==> Skipping modules/awin (non-Windows)"
fi

echo "==> Init/update: modules/ahtt"
git submodule update --init --progress modules/ahtt

echo "==> Init/update: modules/ahtt/modules/3rd-party/args"
git -C modules/ahtt submodule update --init --progress modules/3rd-party/args

echo "Done."
