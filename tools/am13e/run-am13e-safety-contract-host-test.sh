#!/usr/bin/env bash
# Host-only contract test; test constants are fixtures, not E62 board settings.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$root/build-am13e"
mkdir -p "$build/logs"
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$root/src" "$root/tools/am13e/test-am13e-safety-contract.c" \
    -o "$build/am13e-safety-contract-host-test"
"$build/am13e-safety-contract-host-test" 2>&1 \
    | tee "$build/logs/e62-am13e-safety-contract-host.log"
