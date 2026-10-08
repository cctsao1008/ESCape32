#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
    -I"$root/src" "$root/src/esc_math.c" \
    "$root/tools/am13e/test-rel17-esc-math.c" \
    -o "$tmp/test-rel17-esc-math"
"$tmp/test-rel17-esc-math"
echo "[PASS] rel17 portable CRC, scale, smoothing and PID"
