#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
EXPECTED_SYSCFG_DIR="sysconfig_1.28.0"
EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"

TI_ROOT="${TI_ROOT:-$HOME/ti}"
SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"
SYSCFG_ROOT="${SYSCFG_PATH:-$TI_ROOT/$EXPECTED_SYSCFG_DIR}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$TI_ROOT/$EXPECTED_GCC_DIR}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
platform_src="$repo_root/mcu/AM13E23019"

sdk_build_dir="$SDK_ROOT/build/am13e230x/m33_gcc_arm_debug"
work_dir="$repo_root/.am13e-build/motor-probe"
output_dir="$sdk_build_dir/escape32_am13e23019_motor_probe"
hook_file="$work_dir/am13e_motor_probe_hook.cmake"
generated_syscfg="$SDK_ROOT/examples/driverlib/mcpwm/mcpwm_global_load_use_case/am13e230x_lp/m33_nortos/cmake_syscfg_generated"

ti_example_target="ex_mcpwm_global_load_use_case_lp"
target="am13e23019_motor_probe"
elf="$output_dir/$target.elf"
map="$output_dir/$target.map"

"$script_dir/check-env.sh"

echo
echo "Preparing AM13E23019 Stage-B MCPWM backend probe"
echo "================================================"
echo "Repository      : $repo_root"
echo "SDK root        : $SDK_ROOT"
echo "Platform source : $platform_src"
echo "SDK build dir   : $sdk_build_dir"
echo "Output dir      : $output_dir"
echo

# Regenerate a clean, known-good TI SDK GCC build tree.
"$script_dir/build-sdk-smoke.sh"

echo
echo "Building TI MCPWM global-load reference target"
cmake --build "$sdk_build_dir" \
    --target "$ti_example_target" \
    --parallel "$(nproc)"

if ! compgen -G "$generated_syscfg/*.c" > /dev/null; then
    echo "TI MCPWM SysConfig output was not generated: $generated_syscfg" >&2
    exit 1
fi

rm -rf "$work_dir" "$output_dir"
mkdir -p "$work_dir" "$output_dir"

cat > "$hook_file" <<EOF
set(ESCAPE32_AM13E_MOTOR_PROBE_OUTPUT_DIR "$output_dir")
cmake_language(
    DEFER
    CALL include
        "$platform_src/motor_probe.cmake"
)
EOF

echo
echo "Reconfiguring TI SDK with ESCape32 AM13E motor probe"
cmake \
    -S "$SDK_ROOT" \
    -B "$sdk_build_dir" \
    -DCMAKE_PROJECT_INCLUDE="$hook_file"

echo
echo "Building target: $target"
cmake --build "$sdk_build_dir" \
    --target "$target" \
    --parallel "$(nproc)"

[ -f "$elf" ] || {
    echo "Expected ELF was not generated: $elf" >&2
    exit 1
}

objdump="$GCC_ROOT/bin/arm-none-eabi-objdump"
readelf="$GCC_ROOT/bin/arm-none-eabi-readelf"
size="$GCC_ROOT/bin/arm-none-eabi-size"

section_vma() {
    local section="$1"
    "$objdump" -h "$elf" | awk -v s="$section" '$2 == s { print "0x" $4; exit }'
}

intvecs="$(section_vma .intvecs)"
text_vma="$(section_vma .text)"
fail=0

echo
echo "AM13E23019 motor probe ELF validation"
echo "====================================="
"$size" "$elf"
echo

if [ "$intvecs" = "0x00006000" ]; then
    echo "[PASS] .intvecs     $intvecs"
else
    echo "[FAIL] .intvecs     expected 0x00006000, got ${intvecs:-<missing>}"
    fail=1
fi

if [ -n "$text_vma" ] && (( text_vma >= 0x00006100 )); then
    echo "[PASS] .text        $text_vma"
else
    echo "[FAIL] .text        expected >= 0x00006100, got ${text_vma:-<missing>}"
    fail=1
fi

if "$readelf" -h "$elf" | grep -q 'hard-float ABI'; then
    echo "[PASS] ABI          hard-float ABI"
else
    echo "[FAIL] ABI          hard-float ABI not reported"
    fail=1
fi

echo
echo "ELF: $elf"
echo "MAP: $map"

if [ "$fail" -ne 0 ]; then
    echo
    echo "Stage-B MCPWM backend probe failed." >&2
    exit 1
fi

echo
echo "Stage-B MCPWM backend compile probe PASS."
echo "Validated TI APIs:"
echo "  DL_MCPWM_setTimeBasePeriodShadow"
echo "  DL_MCPWM_setCounterCompareShadowValue (1A/1B/2A/2B/3A/3B)"
echo "  DL_MCPWM_setGlobalLoadOneShotLatch"
echo "  DL_MCPWM_setActionQualifierActionShadow (1A/1B/2A/2B/3A/3B)"
echo "  DL_MCPWM_setActionQualifierSWAction (pair-local AQ input force)"
echo "  DL_MCPWM dead-band split-input configuration APIs"
echo "  AQ shadow truth-table encoder (PWM / constant HIGH / constant LOW)"
echo "  six-step shadow encoder (+/-/FLOAT, damp on/off)"
echo "  DL_MCPWM_setActionQualifierActionActive / CompleteActive (static PWM carrier setup)"
echo "  asynchronous AQ software-force six-step selector"
echo "  safe transition ordering (all FLOAT -> B targets -> A targets)"
echo
echo "Architecture selected for next validation:"
echo "  OutA = RED(PWMA), active high"
echo "  OutB = NOT(FED(PWMB))"
echo "  duty/period remain in PWM-boundary shadow domain"
echo "  six-step commutation candidate uses static AQ PWM carriers + AQSFRC"
echo
echo "Not validated yet:"
echo "  AQSFRC release behavior / blanking width / CPU latency on hardware"
echo "  six-step AQ truth table on physical outputs"
echo "  dead-time waveform on scope/logic analyzer"
echo "  output enable/idle behavior"
echo "  BEMF/CMPSS/capture"
