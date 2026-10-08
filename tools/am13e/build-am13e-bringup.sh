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
work_dir="$repo_root/.am13e-build/bringup"
output_dir="$sdk_build_dir/escape32_am13e23019"
hook_file="$work_dir/am13e_project_hook.cmake"
generated_syscfg="$SDK_ROOT/examples/empty/am13e230x_lp/m33_nortos/cmake_syscfg_generated"

target="am13e23019_bringup"
elf="$output_dir/$target.elf"
map="$output_dir/$target.map"

"$script_dir/check-env.sh"

echo
echo "Preparing AM13E23019 Stage-A build"
echo "=================================="
echo "Repository      : $repo_root"
echo "SDK root        : $SDK_ROOT"
echo "Platform source : $platform_src"
echo "SDK build dir   : $sdk_build_dir"
echo "Output dir      : $output_dir"
echo

rm -rf "$work_dir"
mkdir -p "$work_dir"

# First build the pinned TI SDK empty example. Besides proving the SDK baseline,
# this generates the known-good SysConfig C/H files that Stage A reuses.
"$script_dir/build-sdk-smoke.sh"

if ! compgen -G "$generated_syscfg/*.c" > /dev/null; then
    echo "TI empty-example SysConfig output was not generated: $generated_syscfg" >&2
    exit 1
fi

rm -rf "$output_dir"
mkdir -p "$output_dir"

# CMAKE_PROJECT_INCLUDE runs immediately after project(). At that point the TI
# SDK config/source targets do not exist yet, so schedule an include() at the
# end of the top-level directory. Unlike add_subdirectory(), include() is valid
# during deferred execution and lets the repo-owned target bind to TI targets
# after they have been created.
cat > "$hook_file" <<EOF
set(ESCAPE32_AM13E_OUTPUT_DIR "$output_dir")
cmake_language(
    DEFER
    CALL include
        "$platform_src/CMakeLists.txt"
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
objcopy="$GCC_ROOT/bin/arm-none-eabi-objcopy"
readelf="$GCC_ROOT/bin/arm-none-eabi-readelf"
size="$GCC_ROOT/bin/arm-none-eabi-size"
nm="$GCC_ROOT/bin/arm-none-eabi-nm"

section_vma() {
    local section="$1"
    "$objdump" -h "$elf" | awk -v s="$section" '$2 == s { print "0x" $4; exit }'
}

intvecs="$(section_vma .intvecs)"
image_header="$(section_vma .image_header)"
text_vma="$(section_vma .text)"
vtable="$(section_vma .vtable)"
ramfunc="$(section_vma .TI.ramfunc)"

symbol_addr() {
    local symbol="$1"
    "$nm" -n "$elf" | awk -v s="$symbol" '$3 == s { print "0x" $1; exit }'
}

app_start="$(symbol_addr __app_flash_start__)"
app_end="$(symbol_addr __app_flash_end__)"

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
check_equal ".image_header" "$image_header" "0x00006100"
check_equal "app start" "$app_start" "0x00006000"
check_equal "app end" "$app_end" "0x00080000"

# .vtable is contributed by TI's interrupt relocation implementation. The
# interrupt-free Stage-A image may legitimately omit that archive member under
# --gc-sections. If it is linked, its placement must still match the TI
# baseline.
if [ -z "$vtable" ]; then
    printf '[PASS] %-12s not linked (interrupt-free Stage A)\n' ".vtable"
else
    check_equal ".vtable" "$vtable" "0x20000000"
fi

check_equal ".TI.ramfunc" "$ramfunc" "0x00c18000"

if [ -n "$text_vma" ] && (( text_vma >= 0x00006120 )); then
    printf '[PASS] %-12s %s\n' ".text" "$text_vma"
else
    printf '[FAIL] %-12s expected >= 0x00006120, got %s\n' ".text" "${text_vma:-<missing>}"
    fail=1
fi

if "$readelf" -h "$elf" | grep -q 'hard-float ABI'; then
    printf '[PASS] %-12s hard-float ABI\n' "ABI"
else
    printf '[FAIL] %-12s hard-float ABI not reported\n' "ABI"
    fail=1
fi

raw_bin="$output_dir/$target.raw.bin"
packed_bin="$output_dir/$target.e62.bin"
manifest="$output_dir/$target.e62.json"

"$objcopy" -O binary "$elf" "$raw_bin"
python3 "$script_dir/pack-am13e-image.py" \
    "$raw_bin" "$packed_bin" --manifest "$manifest"

echo
echo "ELF:      $elf"
echo "MAP:      $map"
echo "Packed:   $packed_bin"
echo "Manifest: $manifest"

if [ "$fail" -ne 0 ]; then
    echo
    echo "Stage-A ELF validation failed." >&2
    exit 1
fi

echo
echo "Stage-A application-link contract PASS."
echo "Dedicated linker enforces:"
echo "  APP vector @ 0x00006000"
echo "  E62 image header @ 0x00006100 (32 bytes)"
echo "  APP ownership 0x00006000..0x0007FFFF (488 KiB)"
echo "  Boot + FW1 CFG + FW2 CFG remain outside application ownership"
echo "Packager emits an aligned E62 image with header + payload CRC."
