#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
TI_ROOT="${TI_ROOT:-$HOME/ti}"
SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
out_dir="$repo_root/.am13e-build"
out_file="$out_dir/mcpwm-reference.txt"

examples=(
  "mcpwm_basic_pwm_generation"
  "mcpwm_deadband"
  "mcpwm_global_load_use_case"
  "mcpwm_tripzone"
  "mcpwm_performance_cpu_latency"
)

[ -d "$SDK_ROOT" ] || {
    echo "AM13E SDK not found: $SDK_ROOT" >&2
    exit 1
}

mkdir -p "$out_dir"

{
    echo "AM13E230x SDK MCPWM reference capture"
    echo "===================================="
    echo "SDK root: $SDK_ROOT"
    echo

    echo "===== MCPWM DRIVERLIB FILES ====="
    find "$SDK_ROOT/source" "$SDK_ROOT/ti_sdk_config"         -type f \( -iname '*mcpwm*.h' -o -iname '*mcpwm*.c' \)         2>/dev/null | sort
    echo

    while IFS= read -r f; do
        echo
        echo "===== FILE: ${f#$SDK_ROOT/} ====="
        sed -n '1,260p' "$f"
    done < <(
        find "$SDK_ROOT/source" "$SDK_ROOT/ti_sdk_config"             -type f \( -iname '*mcpwm*.h' -o -iname '*mcpwm*.c' \)             2>/dev/null | sort
    )

    for ex in "${examples[@]}"; do
        dir="$SDK_ROOT/examples/driverlib/mcpwm/$ex/am13e230x_lp/m33_nortos"
        echo
        echo "================================================================"
        echo "===== EXAMPLE: $ex ====="
        echo "================================================================"

        if [ ! -d "$dir" ]; then
            echo "[MISSING] $dir"
            continue
        fi

        find "$dir" -maxdepth 1 -type f -printf '%f\n' | sort

        for f in CMakeLists.txt example.syscfg; do
            if [ -f "$dir/$f" ]; then
                echo
                echo "===== FILE: ${dir#$SDK_ROOT/}/$f ====="
                sed -n '1,320p' "$dir/$f"
            fi
        done

        parent="$(dirname "$(dirname "$dir")")"
        for f in "$parent"/*.c "$parent"/*.h; do
            [ -f "$f" ] || continue
            echo
            echo "===== FILE: ${f#$SDK_ROOT/} ====="
            sed -n '1,420p' "$f"
        done
    done
} > "$out_file"

echo "MCPWM reference capture complete:"
echo "$out_file"
echo
echo "Size:"
wc -l -c "$out_file"
