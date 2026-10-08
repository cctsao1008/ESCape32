#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
sdk="${AM13E_SDK_ROOT:-$HOME/ti/am13e230x_sdk_26_01_00_03}"
gcc="${GCC_ARM_TOOLCHAIN_PATH:-$HOME/ti/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi}"
elf="${1:-$sdk/build/am13e230x/m33_gcc_arm_debug/escape32_am13e23019_boot/am13e23019_boot.elf}"
if [[ ! -f "$elf" ]]; then
  echo "[FAIL] missing Boot ELF: $elf; run build-am13e-boot.sh first" >&2
  exit 1
fi
python3 "$root/tools/am13e/audit-boot-ramfunc.py" \
  --elf "$elf" --toolchain-bin "$gcc/bin"
