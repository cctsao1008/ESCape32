#!/usr/bin/env bash
set -euo pipefail
: "${AM13E_SDK_ROOT:?Set official SDK path}"
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
out=$(mktemp -d /tmp/e62-prompt-pendsv.XXXXXX)
trap 'rm -rf "$out"' EXIT
for mode in default prompt; do
    extra=()
    if [ "$mode" = prompt ]; then extra=(-DAM13E_EXPERIMENTAL_SC5_PROMPT_PRODUCER=1); fi
    cc "${extra[@]}" -std=c11 -O1 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections \
      -DAM13E -DAM13E_BOARD_MCU_PART=23019 -DAM13E_BOARD_PACKAGE=1 -DCLK=200000000 \
      "-DAM13E_TEST_TI_GPIO_HEADER=\"$AM13E_SDK_ROOT/source/driverlib/am13e230x/dl_gpio.h\"" \
      -Isrc -Imcu/AM13E/serial -Imcu/AM13E -Imcu/AM13E/hal \
      -Imcu/_support/AM13E/tests/hal/register_mock \
      -I"$AM13E_SDK_ROOT/source/device/am13e230x/include/hw" \
      -I"$AM13E_SDK_ROOT/source/driverlib/am13e230x" \
      mcu/AM13E/hal/irq_vectors.c mcu/_support/AM13E/tests/app/prompt_pendsv_host_test.c -o "$out/$mode"
    "$out/$mode"
done
