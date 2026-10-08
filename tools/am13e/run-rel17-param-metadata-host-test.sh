#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
    -I"$root/src" "$root/src/esc_cmd_parse.c" \
    "$root/src/esc_param_metadata.c" \
    "$root/tools/am13e/test-rel17-param-metadata.c" \
    -o "$tmp/param-metadata-test"
"$tmp/param-metadata-test"
echo "[PASS] ESCape32 rel17 47 canonical parameter IDs/names and lookup"
