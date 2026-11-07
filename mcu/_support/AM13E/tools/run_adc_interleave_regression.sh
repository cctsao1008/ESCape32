#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
out=$(mktemp -d /tmp/e62-first-edge.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-DLED_CNT=0 -D_GNU_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Imcu/AM13E/serial -Imcu/AM13E -Imcu/AM13E/hal -Imcu/_support/AM13E/tests/app -Imcu/_support/AM13E/tests/hal/motor_mock -fsanitize=undefined -fno-sanitize-recover=all)
sources=(mcu/_support/AM13E/tests/app/adc_interleave_source_host_test.c)
for mode in default experimental; do
    extra=()
    if [ "$mode" = experimental ]; then extra=(-DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_ADC_INTERLEAVE=1); fi
    cc "${flags[@]}" "${extra[@]}" "${sources[@]}" -o "$out/$mode"
    "$out/$mode"
done
if cc "${flags[@]}" -DAM13E_SIM_ONLY_ADC_INTERLEAVE=1 "${sources[@]}" -o "$out/forbidden" >"$out/guard.log" 2>&1; then
    echo 'FAIL: experiment compiled without SIM_ONLY'; exit 1
fi
rg -q 'ADC interleave fault-isolation fixture requires explicit AM13E SIM_ONLY profile' "$out/guard.log"
echo 'PASS actual PendSV default/interleaved cadence and SIM_ONLY compile guard'
