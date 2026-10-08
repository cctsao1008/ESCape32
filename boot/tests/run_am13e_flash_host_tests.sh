#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-am13e-host-tests}"
mkdir -p "$BUILD"
: "${CC:=cc}"
"$CC" -std=c11 -O1 -g -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -DAM13E_FLASH_TEST \
  -I"$ROOT/boot/tests/am13e_mock" \
  "$ROOT/boot/tests/am13e_flash_transaction_test.c" \
  "$ROOT/boot/mcu/AM13E/flash.c" \
  -o "$BUILD/am13e_flash_transaction_test"
"$BUILD/am13e_flash_transaction_test"
