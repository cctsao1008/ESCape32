#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
out=$(mktemp -d /tmp/e62-coast-phase.XXXXXX)
trap 'rm -rf "$out"' EXIT
flags=(-D_GNU_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -DAM13E -DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_COAST_PHASE_ACQUIRE=1 -Imcu/AM13E/hal -Isrc -fsanitize=undefined -fno-sanitize-recover=all)
cc "${flags[@]}" mcu/AM13E/hal/coast_phase_plan.c mcu/_support/AM13E/tests/hal/coast_phase_plan_host_test.c -o "$out/planner"
"$out/planner"
flags+=(-DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Imcu/AM13E/serial -Imcu/AM13E -Imcu/_support/AM13E/tests/app -Imcu/_support/AM13E/tests/hal/motor_mock -DAM13E_SIM_ONLY_FIRST_EDGE_QUARTER=1 -DAM13E_SIM_ONLY_FIRST_EDGE_SEED=1 -DAM13E_SIM_ONLY_ACQUISITION_QUARTER=1 -DAM13E_SIM_ONLY_ACQUISITION_DIRECT_INTERVAL=1)
cc "${flags[@]}" src/util.c mcu/AM13E/hal/motor_bemf_event_plan.c mcu/AM13E/hal/motor_phase_plan.c mcu/_support/AM13E/tests/app/motor_source_host_test.c -o "$out/core"
"$out/core"
