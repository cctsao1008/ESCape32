#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Build TI SDK and generate the application CMake hook using the existing
# validated E62 Stage-A workflow, then build the independently linked FW1 ELF.
bash "$root/tools/am13e/build-am13e-bringup.sh"

sdk="${AM13E_SDK_ROOT:-${TI_ROOT:-$HOME/ti}/am13e230x_sdk_26_01_00_03}"
build="$sdk/build/am13e230x/m33_gcc_arm_debug"
target=am13e23019_fw1
cmake --build "$build" --target "$target" --parallel "$(nproc)"

elf="$build/escape32_am13e23019/$target.elf"
test -s "$elf" || { echo "[FAIL] missing FW1 ELF: $elf" >&2; exit 1; }
toolchain="${GCC_ARM_TOOLCHAIN_PATH:-${TI_ROOT:-$HOME/ti}/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi}"
objdump="$toolchain/bin/arm-none-eabi-objdump"
nm="$toolchain/bin/arm-none-eabi-nm"
size="$toolchain/bin/arm-none-eabi-size"
"$size" "$elf"
"$objdump" -h "$elf" | grep -Eq '^[[:space:]]*[0-9]+[[:space:]]+\.intvecs[[:space:]]+.*00006000' || {
    echo "[FAIL] FW1 vector base is not 0x6000" >&2; exit 1;
}
"$nm" "$elf" | grep -q ' fw1_debug_step$' || {
    echo "[FAIL] FW1 runtime debug step control missing" >&2; exit 1;
}
"$nm" "$elf" | grep -q ' fw1_debug_enable$' || {
    echo "[FAIL] FW1 runtime debug enable control missing" >&2; exit 1;
}
echo "[PASS] E62 FW1 MCPWM0 runtime ELF linked at APP_BASE 0x6000"
echo "[NOTE] Full ESCape32 runtime, MCPWM0 product pinmux, trip-zone and BEMF ISR still pending"
