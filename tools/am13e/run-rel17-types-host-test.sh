#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
    -I"$root/src" "$root/tools/am13e/test-rel17-types.c" \
    -o "$tmp/test-rel17-types"
"$tmp/test-rel17-types"
echo "[PASS] rel17 portable Cfg/PID data model"
