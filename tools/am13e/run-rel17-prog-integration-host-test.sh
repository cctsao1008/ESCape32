#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=gnu11 -D_GNU_SOURCE -DESCAPE32_PROG_HOST_TEST \
    -O2 -Wall -Wextra -Werror -Wno-unused-parameter \
    -I"$root/src" -I"$root/tools/am13e" \
    "$root/src/prog.c" "$root/src/esc_cmd_parse.c" \
    "$root/src/esc_config.c" "$root/src/esc_param_access.c" \
    "$root/src/esc_param_metadata.c" \
    "$root/tools/am13e/test-rel17-prog-integration.c" \
    -o "$tmp/rel17-prog-integration"
"$tmp/rel17-prog-integration"
echo "[PASS] original rel17 prog.c CLI and CRSF integration"
