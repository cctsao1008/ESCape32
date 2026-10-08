#!/usr/bin/env bash
# E62 real rel17 main.c compilation diagnostic; logs stay inside build dir.
set -uo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build="${1:-build-am13e}"
case "$build" in /*) ;; *) build="$root/$build" ;; esac
mkdir -p "$build/logs"
log="$build/logs/e62-rel17-main-build.log"
echo "[INFO] Capturing real rel17 firmware build to $log"
set -o pipefail
cmake --build "$build" --target E62 -j1 2>&1 | tee "$log"
