#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"

TI_ROOT="${TI_ROOT:-$HOME/ti}"
SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$TI_ROOT/$EXPECTED_GCC_DIR}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
boot_src="$repo_root/boot/mcu/AM13E23019"

sdk_build_dir="$SDK_ROOT/build/am13e230x/m33_gcc_arm_debug"
work_dir="$repo_root/.am13e-build/boot"
output_dir="$sdk_build_dir/escape32_am13e23019_boot"
hook_file="$work_dir/am13e_boot_hook.cmake"

target="am13e23019_boot"
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
echo "Reconfiguring TI SDK with AM13E23019 Boot target"
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
ramfunc="$(section_vma .TI.ramfunc)"
boot_start="$(symbol_addr __boot_flash_start__)"
boot_end="$(symbol_addr __boot_flash_end__)"
bank0_erase="$(symbol_addr boot_flash_bank0_erase_sector)"
bank0_program="$(symbol_addr boot_flash_bank0_program)"
flash_exec="$(symbol_addr DL_FlashCTL_executeCommand)"
fail=0

check_equal() {
    local label="$1"
    local actual="$2"
    local expected="$3"
    if [ "$actual" = "$expected" ]; then
        printf '[PASS] %-20s %s\n' "$label" "$actual"
    else
        printf '[FAIL] %-20s expected %s, got %s\n' "$label" "$expected" "${actual:-<missing>}"
        fail=1
    fi
}

check_symbol() {
    local symbol="$1"
    if "$nm" "$elf" | grep -q " $symbol$"; then
        printf '[PASS] %-20s linked\n' "$symbol"
    else
        printf '[FAIL] %-20s missing\n' "$symbol"
        fail=1
    fi
}

echo
echo "AM13E23019 Boot ELF validation"
echo "====================================="
"$size" "$elf"
echo

check_equal ".intvecs" "$intvecs" "0x00000000"
check_equal "boot start" "$boot_start" "0x00000000"
check_equal "boot end" "$boot_end" "0x00004000"

check_symbol "boot_port_app_valid"
check_symbol "boot_port_jump_to_app"
check_symbol "boot_port_map_read"
check_symbol "boot_port_write_block"
check_symbol "boot_flash_app_range_valid"
check_symbol "boot_flash_erase_sector"
check_symbol "boot_flash_program"
check_symbol "boot_flash_verify"
check_symbol "boot_protocol_run"
check_symbol "boot_service_recv_value"
check_symbol "boot_service_send_value"
check_symbol "boot_service_recv_data"
check_symbol "boot_service_send_data"
check_symbol "boot_service_port_bind"
check_symbol "boot_service_port_crc32"

check_ram_symbol() {
    local label="$1"
    local address="$2"
    if [ -n "$address" ] &&
       (( address >= 0x00c18000 && address < 0x00c20000 )); then
        printf '[PASS] %-20s %s\n' "$label" "$address"
    else
        printf '[FAIL] %-20s expected RAM_C, got %s\n' "$label" "${address:-<missing>}"
        fail=1
    fi
}

check_ram_symbol "bank0 erase" "$bank0_erase"
check_ram_symbol "bank0 program" "$bank0_program"
check_ram_symbol "Flash cmd exec" "$flash_exec"

if [ -n "$ramfunc" ] && (( ramfunc >= 0x00c18000 && ramfunc < 0x00c20000 )); then
    printf '[PASS] %-20s %s\n' ".TI.ramfunc" "$ramfunc"
else
    printf '[FAIL] %-20s expected RAM_C, got %s\n' ".TI.ramfunc" "${ramfunc:-<missing>}"
    fail=1
fi

if "$readelf" -h "$elf" | grep -q 'hard-float ABI'; then
    printf '[PASS] %-20s hard-float ABI\n' "ABI"
else
    printf '[FAIL] %-20s hard-float ABI not reported\n' "ABI"
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
echo "  AM13E boot platform adapter linked"
echo "  minimum APP vector sanity check"
echo "  direct VTOR/MSP/reset-entry handoff scaffold"
echo "  APP-range guarded Flash wrapper linked"
echo "  common ESCape32 boot protocol engine linked"
echo "  common ESCape32 service framing linked"
echo "  AM13E service transport adapter linked"
echo "  Bank1 erase/program DriverLib path linked"
echo "  Bank0 erase/program transaction linked in RAM_C"
echo "  TI Flash command executor linked in RAM_C"
echo
echo "Not implemented yet:"
echo "  physical service transport binding (PB14 reuse is pending review)"
echo "  image header / CRC / valid-record policy"
echo "  boot-entry request handshake"
