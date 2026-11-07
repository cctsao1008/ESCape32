#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
out=$(mktemp -d /tmp/e62-first-edge.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-D_GNU_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Imcu/AM13E/serial -Imcu/AM13E -Imcu/AM13E/hal -Imcu/_support/AM13E/tests/app -Imcu/_support/AM13E/tests/hal/motor_mock -fsanitize=undefined -fno-sanitize-recover=all)
sources=(src/util.c mcu/AM13E/hal/motor_bemf_event_plan.c mcu/AM13E/hal/motor_phase_plan.c mcu/_support/AM13E/tests/app/motor_source_host_test.c)
for mode in default experimental seed acquisition; do
    extra=()
    if [ "$mode" = experimental ]; then extra=(-DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_FIRST_EDGE_QUARTER=1); fi
    if [ "$mode" = seed ]; then extra=(-DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_FIRST_EDGE_QUARTER=1 -DAM13E_SIM_ONLY_FIRST_EDGE_SEED=1); fi
    if [ "$mode" = acquisition ]; then extra=(-DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_FIRST_EDGE_QUARTER=1 -DAM13E_SIM_ONLY_FIRST_EDGE_SEED=1 -DAM13E_SIM_ONLY_ACQUISITION_QUARTER=1); fi
    cc "${flags[@]}" "${extra[@]}" "${sources[@]}" -o "$out/$mode"
    "$out/$mode"
done
if cc "${flags[@]}" -DAM13E_SIM_ONLY_FIRST_EDGE_QUARTER=1 "${sources[@]}" -o "$out/forbidden" >"$out/guard.log" 2>&1; then
    echo 'FAIL: experiment compiled without SIM_ONLY'; exit 1
fi
rg -q 'First-edge policy experiment requires explicit SIM_ONLY profile' "$out/guard.log"
echo 'PASS default + experimental actual-Core safety/overflow/timeout/boundary regression, production guard'
if cc "${flags[@]}" -DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_ACQUISITION_QUARTER=1 "${sources[@]}" -o "$out/missing-seed" >"$out/seed-guard.log" 2>&1; then
    echo 'FAIL: acquisition variant compiled without prerequisite seed experiment'; exit 1
fi
rg -q 'Acquisition-window experiment requires the isolated first-edge seed experiment' "$out/seed-guard.log"
if cc "${flags[@]}" -DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_FIRST_EDGE_SEED=1 "${sources[@]}" -o "$out/missing-first-edge" >"$out/first-edge-guard.log" 2>&1; then
    echo 'FAIL: seed variant compiled without prerequisite acceptance experiment'; exit 1
fi
rg -q 'First-edge seed experiment requires the isolated first-edge acceptance experiment' "$out/first-edge-guard.log"
echo 'PASS experimental policy prerequisite guards'
