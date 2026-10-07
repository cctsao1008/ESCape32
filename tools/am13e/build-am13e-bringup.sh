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
platform_src="$repo_root/mcu/am13e23019"

sdk_build_dir="$SDK_ROOT/build/am13e230x/m33_gcc_arm_debug"
work_dir="$repo_root/.am13e-build/bringup"
overlay_src="$work_dir/source"
overlay_bin="$sdk_build_dir/escape32_am13e23019"
hook_file="$work_dir/am13e_project_hook.cmake"
seed_syscfg="$SDK_ROOT/examples/empty/am13e230x_lp/m33_nortos/example.syscfg"

target="am13e23019_bringup"
elf="$overlay_bin/$target.elf"
map="$overlay_bin/$target.map"

"$script_dir/check-env.sh"

[ -f "$seed_syscfg" ] || {
    echo "TI SDK SysConfig seed not found: $seed_syscfg" >&2
    exit 1
}

echo
echo "Preparing AM13E23019 Stage-A build overlay"
echo "=========================================="
echo "Repository      : $repo_root"
echo "SDK root        : $SDK_ROOT"
echo "Overlay source  : $overlay_src"
echo "SDK build dir   : $sdk_build_dir"
echo

rm -rf "$work_dir"
mkdir -p "$overlay_src/src"

cp "$platform_src/CMakeLists.txt" "$overlay_src/CMakeLists.txt"
cp "$platform_src/src/main.c" "$overlay_src/src/main.c"
cp "$seed_syscfg" "$overlay_src/example.syscfg"

# First generate the pinned TI SDK GCC baseline. This gives us a known-good
# cache with TI's exact SDK device/compiler/SysConfig configuration.
"$script_dir/build-sdk-smoke.sh"

# Add the external AM13E target only for the reconfigure below. CMake's project
# include runs immediately after project(); DEFER schedules add_subdirectory()
# until the TI top-level directory has created ti_sdk_config/source targets.
cat > "$hook_file" <<EOF
cmake_language(
    DEFER
    DIRECTORY "$SDK_ROOT"
    CALL add_subdirectory
        "$overlay_src"
        "$overlay_bin"
)
EOF

echo
echo "Reconfiguring TI SDK with AM13E23019 bring-up target"
cmake     -S "$SDK_ROOT"     -B "$sdk_build_dir"     -DCMAKE_PROJECT_INCLUDE="$hook_file"

echo
echo "Building target: $target"
cmake --build "$sdk_build_dir"     --target "$target"     --parallel "$(nproc)"

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
vtable="$(section_vma .vtable)"
ramfunc="$(section_vma .TI.ramfunc)"

fail=0

check_equal() {
    local label="$1"
    local actual="$2"
    local expected="$3"

    if [ "$actual" = "$expected" ]; then
        printf '[PASS] %-12s %s\n' "$label" "$actual"
    else
        printf '[FAIL] %-12s expected %s, got %s\n' "$label" "$expected" "${actual:-<missing>}"
        fail=1
    fi
}

echo
echo "AM13E23019 ELF validation"
echo "========================="
"$size" "$elf"
echo

check_equal ".intvecs" "$intvecs" "0x00006000"
check_equal ".vtable" "$vtable" "0x20000000"
check_equal ".TI.ramfunc" "$ramfunc" "0x00c18000"

if [ -n "$text_vma" ] && (( text_vma >= 0x00006100 )); then
    printf '[PASS] %-12s %s\n' ".text" "$text_vma"
else
    printf '[FAIL] %-12s expected >= 0x00006100, got %s\n' ".text" "${text_vma:-<missing>}"
    fail=1
fi

if "$readelf" -h "$elf" | grep -q 'hard-float ABI'; then
    printf '[PASS] %-12s hard-float ABI\n' "ABI"
else
    printf '[FAIL] %-12s hard-float ABI not reported\n' "ABI"
    fail=1
fi

echo
echo "ELF: $elf"
echo "MAP: $map"

if [ "$fail" -ne 0 ]; then
    echo
    echo "Stage-A ELF validation failed." >&2
    exit 1
fi

echo
echo "Stage-A relocatable application baseline PASS."
