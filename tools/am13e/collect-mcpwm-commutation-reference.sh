#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
TI_ROOT="${TI_ROOT:-$HOME/ti}"
SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="$repo_root/.am13e-build"
out_file="$out_dir/mcpwm-commutation-reference.txt"

hdr="$SDK_ROOT/source/driverlib/am13e230x/dl_mcpwm.h"
src="$SDK_ROOT/source/driverlib/am13e230x/dl_mcpwm.c"
hw="$SDK_ROOT/source/device/am13e230x/include/hw/hw_mcpwm.h"
syscfg="$SDK_ROOT/examples/driverlib/mcpwm/mcpwm_global_load_use_case/am13e230x_lp/m33_nortos/example.syscfg"

for f in "$hdr" "$src" "$hw" "$syscfg"; do
    [ -f "$f" ] || {
        echo "Missing SDK file: $f" >&2
        exit 1
    }
done

mkdir -p "$out_dir"

{
    echo "AM13E230x MCPWM commutation/global-load reference"
    echo "================================================="
    echo "SDK root: $SDK_ROOT"
    echo

    echo "===== DriverLib: global-load / one-shot / software-force declarations ====="
    grep -n -B 12 -A 24 -E \
        'GlobalLoad|global load|GLOBAL_LOAD|OneShot|one shot|GFRCLD|GLDOSHT|GLDMODE' \
        "$hdr" || true

    echo
    echo "===== DriverLib implementation: load-mode/global-load configuration ====="
    grep -n -B 12 -A 40 -E \
        'GlobalLoad|global load|enableOneShot|triggerEvent|GFRCLD|GLDOSHT|GLDMODE' \
        "$src" || true

    echo
    echo "===== HW register definitions: GLDCTL / GLDOSHTCTL / Trip force ====="
    grep -n -B 5 -A 25 -E \
        'GLDCTL|GLDOSHTCTL|GFRCLD|OSHTLD|OSHTCLR|TZFRC|TZ.*FORCE|INTFRC' \
        "$hw" || true

    echo
    echo "===== TI global-load example SysConfig ====="
    grep -n -B 8 -A 28 -E \
        'loadMode|GlobalLoad|OneShot|triggerEvent|deadBand|counterCompare|actionQualifier' \
        "$syscfg" || true
} > "$out_file"

echo "MCPWM commutation/global-load reference capture complete:"
echo "$out_file"
echo
wc -l -c "$out_file"
