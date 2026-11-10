#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
out=$(mktemp -d /tmp/e62-phase-witness.XXXXXX)
trap 'rm -rf "$out"' EXIT
cc -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -DAM13E -DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_COAST_PHASE_ACQUIRE=1 -Imcu/AM13E/hal -Isrc -fsanitize=undefined -fno-sanitize-recover=all mcu/AM13E/hal/coast_phase_plan.c mcu/_support/AM13E/tests/hal/coast_phase_ambiguity_witness.c -o "$out/witness"
"$out/witness"
