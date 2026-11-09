#!/usr/bin/env bash
# Only new ambiguity/contradictory-epoch counterexamples, UNIT scope.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
out=$(mktemp -d /tmp/e62-coast-uncertainty.XXXXXX)
trap 'rm -rf "$out"' EXIT
cc -D_GNU_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -DAM13E -DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_SIM_ONLY_COAST_PHASE_ACQUIRE=1 -Imcu/AM13E/hal -Isrc -fsanitize=undefined -fno-sanitize-recover=all mcu/AM13E/hal/coast_phase_plan.c mcu/_support/AM13E/tests/hal/coast_phase_uncertainty_host_test.c -o "$out/planner"
for n in 0 1 2 3; do "$out/planner" "$n"; done
