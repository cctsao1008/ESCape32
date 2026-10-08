#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"

TI_ROOT="${TI_ROOT:-$HOME/ti}"
SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$TI_ROOT/$EXPECTED_GCC_DIR}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
boot_src="$repo_root/boot/mcu/am13e23019"

sdk_build_dir="$SDK_ROOT/build/am13e230x/m33_gcc_arm_debug"
work_dir="$repo_root/.am13e-build/boot"
output_dir="$sdk_build_dir/escape32_am13e23019_boot"
hook_file="$work_dir/am13e_boot_hook.cmake"

target="am13e23019_e62_boot"
elf="$output_dir/$target.elf"
map="$output_dir/$target.map"

"$script_dir/check-env.sh"
"$script_dir/build-sdk-smoke.sh"

rm -rf "$work_dir" "$output_dir"
mkdir -p "$work_dir" "$output_dir"

cat > "$hook_file" <<EOF
set(ESCAPE32_AM13E_BOOT_OUTPUT_DIR "$output_dir")
cmake_language(
    DEFER
    CALL include
        "$boot_src/CMakeLists.txt"
)
EOF

echo
echo "Reconfiguring TI SDK with E62 Boot target"
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
nm="$GCC_ROOT/bin/arm-none-eabi-nm"
size="$GCC_ROOT/bin/arm-none-eabi-size"

section_vma() {
    local section="$1"
    "$objdump" -h "$elf" | awk -v s="$section" '$2 == s { print "0x" $4; exit }'
}

symbol_addr() {
    local symbol="$1"
    "$nm" -n "$elf" | awk -v s="$symbol" '$3 == s { print "0x" $1; exit }'
}

intvecs="$(section_vma .intvecs)"
boot_start="$(symbol_addr __e62_boot_flash_start__)"
boot_end="$(symbol_addr __e62_boot_flash_end__)"
fail=0

check_equal() {
    local label="$1"
    local actual="$2"
    local expected="$3"
    if [ "$actual" = "$expected" ]; then
        printf '[PASS] %-14s %s\n' "$label" "$actual"
    else
        printf '[FAIL] %-14s expected %s, got %s\n' "$label" "$expected" "${actual:-<missing>}"
        fail=1
    fi
}

echo
echo "E62 AM13E23019 Boot ELF validation"
echo "====================================="
"$size" "$elf"
echo

check_equal ".intvecs" "$intvecs" "0x00000000"
check_equal "boot start" "$boot_start" "0x00000000"
check_equal "boot end" "$boot_end" "0x00004000"

if "$nm" "$elf" | grep -q ' e62_boot_app_vector_sane$'; then
    echo "[PASS] app validate    linked"
else
    echo "[FAIL] app validate    missing"
    fail=1
fi

if "$nm" "$elf" | grep -q ' e62_boot_jump_to_app$'; then
    echo "[PASS] app jump        linked"
else
    echo "[FAIL] app jump        missing"
    fail=1
fi

if "$readelf" -h "$elf" | grep -q 'hard-float ABI'; then
    echo "[PASS] ABI             hard-float ABI"
else
    echo "[FAIL] ABI             hard-float ABI not reported"
    fail=1
fi

echo
echo "ELF: $elf"
echo "MAP: $map"

if [ "$fail" -ne 0 ]; then
    echo
    echo "Boot ELF validation failed." >&2
    exit 1
fi

echo
echo "Boot image contract PASS."
echo "Validated:"
echo "  boot vector @ 0x00000000"
echo "  boot flash ownership 0x00000000..0x00003FFF"
echo "  minimum APP vector sanity check"
echo "  direct VTOR/MSP/reset-entry handoff scaffold"
echo
echo "Not implemented yet:"
echo "  PB14 service/programming protocol"
echo "  image header / CRC / valid-record policy"
echo "  APP erase/program/verify"
echo "  boot-entry request handshake"
