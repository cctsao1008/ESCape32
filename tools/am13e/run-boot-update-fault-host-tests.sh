#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O0 -Wall -Wextra -Werror \
  -I"$root/boot/src" -I"$root/boot/mcu/AM13E23019/src" \
  "$root/tools/am13e/test_boot_update_fault_host.c" \
  "$root/boot/mcu/AM13E23019/src/boot_update.c" \
  -o "$tmp/test_boot_update_fault_host"
"$tmp/test_boot_update_fault_host"
