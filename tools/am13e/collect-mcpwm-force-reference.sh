#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
TI_ROOT="${TI_ROOT:-$HOME/ti}"
SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="$repo_root/.am13e-build"
out_file="$out_dir/mcpwm-force-reference.txt"

hdr="$SDK_ROOT/source/driverlib/am13e230x/dl_mcpwm.h"
src="$SDK_ROOT/source/driverlib/am13e230x/dl_mcpwm.c"
hw="$SDK_ROOT/source/device/am13e230x/include/hw/hw_mcpwm.h"

for f in "$hdr" "$src" "$hw"; do
    [ -f "$f" ] || {
        echo "Missing SDK file: $f" >&2
        exit 1
    }
done

mkdir -p "$out_dir"

{
    echo "AM13E230x MCPWM force/dead-band API reference"
    echo "============================================="
    echo "SDK root: $SDK_ROOT"
    echo

    for f in "$hdr" "$src" "$hw"; do
        echo
        echo "===== FILE: ${f#$SDK_ROOT/} ====="

        grep -n -i -C 5 -E             'software[ _-]*force|one[ _-]*time[ _-]*force|AQ.*FORCE|AQ.*SFRC|AQOTSFRC|AQCSFRC|setActionQualifier|configureActionQualifier|dead[ _-]*band|configureDeadBand|setRisingEdgeDelay|setFallingEdgeDelay|DBCTL|OUT_MODE|POLSEL|IN_MODE|OUTSWAP'             "$f" || true
    done
} > "$out_file"

echo "MCPWM force/dead-band reference capture complete:"
echo "$out_file"
echo
wc -l -c "$out_file"
