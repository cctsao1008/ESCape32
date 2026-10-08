#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
  -I"$root/src" "$root/tools/am13e/test-rel17-am13e-bemf-integration.c" \
  -o "$tmp/test-rel17-am13e-bemf-integration"
"$tmp/test-rel17-am13e-bemf-integration"
echo "[PASS] AM13E BEMF event adapter uses rel17 state and sector policy"
