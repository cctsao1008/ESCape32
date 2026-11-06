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
    extra=(-DAM13E_REFERENCE_DIAGNOSTICS=1 -DAM13E_SIM_ONLY_BOARD_PROFILE=1 -DAM13E_EXPERIMENTAL_SC5_EQUIVALENT_REFRESH=1)
  fi
done
exec "$E62_SC5_HOST_CC" "${extra[@]}" "$@"
CC
chmod +x "$wrappers/cc"
cd "$repo"
PATH="$wrappers:$PATH" bash mcu/_support/AM13E/tools/run_host_regressions.sh full
