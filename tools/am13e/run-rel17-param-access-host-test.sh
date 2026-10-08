#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
    -I"$root/src" "$root/src/esc_cmd_parse.c" \
    "$root/src/esc_param_metadata.c" "$root/src/esc_config.c" \
    "$root/src/esc_param_access.c" \
    "$root/tools/am13e/test-rel17-param-access.c" \
    -o "$tmp/param-access-test"
"$tmp/param-access-test"
echo "[PASS] ESCape32 rel17 numeric get/set validate readback and rejection"
