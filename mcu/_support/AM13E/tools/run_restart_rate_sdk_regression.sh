#!/usr/bin/env bash
# Explicit experiment: default firmware and all non-SC5 configurations unchanged.
set -euo pipefail
: "${AM13E_SDK_ROOT:?Set AM13E_SDK_ROOT to the verified TI SDK}"
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
compiler=$(command -v cc)
wrappers=$(mktemp -d /tmp/e62-sc5-cc.XXXXXX)
trap 'rm -rf -- "$wrappers"' EXIT
export E62_SC5_HOST_CC="$compiler"
cat > "$wrappers/cc" <<'CC'
#!/usr/bin/env bash
set -euo pipefail
extra=()
for arg in "$@"; do
  if [[ "$arg" == '-DAM13E_SC5_FIXTURE' ]]; then
    extra=(-DAM13E_SIM_ONLY_FIRST_EDGE_QUARTER=1 -DAM13E_SIM_ONLY_FIRST_EDGE_SEED=1 -DAM13E_SIM_ONLY_ACQUISITION_QUARTER=1 -DAM13E_SIM_ONLY_ACQUISITION_DIRECT_INTERVAL=1 -DAM13E_SIM_ONLY_RESTART_PHASE_RATE=1 -DAM13E_EXPERIMENTAL_SC5_CONTINUOUS_EQUIVALENT=1 -DAM13E_EXPERIMENTAL_SC5_PROMPT_PRODUCER=1 -DAM13E_REFERENCE_DIAGNOSTICS=1 -DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_EXPERIMENTAL_SC5_EQUIVALENT_REFRESH=1 -DAM13E_EXPERIMENTAL_SC5_CHANGED_REFRESH=1 -DAM13E_EXPERIMENTAL_SC5_SETTLE_ZEROS=2)
  fi
done
if [ "${#extra[@]}" -gt 0 ]; then
    exec "$E62_SC5_HOST_CC" "${extra[@]}" "$@" mcu/AM13E/hal/restart_phase_rate.c -lm
fi
exec "$E62_SC5_HOST_CC" "$@"
CC
chmod +x "$wrappers/cc"
cd "$repo"
if [ "${1:-full}" = sc5-only ]; then
    E62_MOTOR_REGISTER_FIXTURES=sc5 PATH="$wrappers:$PATH" bash mcu/_support/AM13E/tools/run_motor_register_regression.sh
else
    PATH="$wrappers:$PATH" bash mcu/_support/AM13E/tools/run_host_regressions.sh full
fi
