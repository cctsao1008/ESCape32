#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$root/build-am13e"
mkdir -p "$build/logs"
cc="${CC:-cc}"
"$cc" -std=c11 -O2 -Wall -Wextra -Werror -I"$root/src" \
    "$root/tools/am13e/test-rel17-bemf-bridge.c" \
    -o "$build/rel17-bemf-bridge-host-test"
"$build/rel17-bemf-bridge-host-test"
