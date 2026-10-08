#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
    -I"$root/src" "$root/tools/am13e/test-six-step-policy.c" \
    -o "$tmp/test-six-step-policy"
"$tmp/test-six-step-policy"
echo "[PASS] ESCape32 rel17 six-step policy (forward/reverse, 6 sectors)"
