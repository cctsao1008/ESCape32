#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O0 -Wall -Wextra -Werror \
  -I"$root/boot/src" \
  "$root/tools/am13e/test_boot_recovery_host.c" \
  "$root/boot/src/protocol.c" \
  -o "$tmp/test_boot_recovery_host"
"$tmp/test_boot_recovery_host"
