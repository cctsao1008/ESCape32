#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
  -I"$root/src" "$root/src/esc_config.c" \
  "$root/tools/am13e/test-rel17-config.c" -o "$tmp/config-test"
"$tmp/config-test"
echo "[PASS] ESCape32 rel17 config normalization and idempotence"
