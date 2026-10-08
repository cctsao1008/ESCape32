#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
toolchain="${GCC_ARM_TOOLCHAIN_PATH:-$HOME/ti/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi}"
gcc="$toolchain/bin/arm-none-eabi-gcc"
objcopy="$toolchain/bin/arm-none-eabi-objcopy"
gdb="${GDB:-gdb-multiarch}"
qemu="${QEMU:-qemu-system-arm}"
port="${QEMU_GDB_PORT:-12349}"
tmp="$(mktemp -d)"
qemu_pid=""
cleanup() { if [[ -n "$qemu_pid" ]]; then kill "$qemu_pid" 2>/dev/null || true; wait "$qemu_pid" 2>/dev/null || true; fi; rm -rf "$tmp"; }
trap cleanup EXIT
command -v "$gdb" >/dev/null || { echo "[FAIL] missing $gdb (install gdb-multiarch)" >&2; exit 1; }
command -v "$qemu" >/dev/null || { echo "[FAIL] missing $qemu" >&2; exit 1; }
"$gcc" -mcpu=cortex-m33 -mthumb -nostdlib -nostartfiles -g3 \
  -Wl,--build-id=none -Wl,-T,"$root/tools/am13e/qemu-m33-handoff.ld" \
  "$root/tools/am13e/qemu-m33-handoff.S" -o "$tmp/handoff.elf"
"$objcopy" -O binary "$tmp/handoff.elf" "$tmp/handoff.bin"
cat > "$tmp/test.gdb" <<EOF
set confirm off
set pagination off
set architecture arm
file $tmp/handoff.elf
target remote localhost:$port
printf "[INFO] reset PC=0x%x MSP=0x%x\\n", $pc, $msp
hbreak app_landed
continue
set \$vtor = *(unsigned int*)0xE000ED08
set \$marker = *(unsigned int*)0x20000100
if \$vtor != 0x6000
  printf "[FAIL] VTOR = 0x%x\\n", \$vtor
  quit 1
end
if \$marker != 0x5a
  printf "[FAIL] APP marker = 0x%x\\n", \$marker
  quit 1
end
if \$msp != 0x20008000
  printf "[FAIL] MSP = 0x%x\\n", \$msp
  quit 1
end
printf "[PASS] QEMU Cortex-M33 reached APP at 0x6000; VTOR / MSP / APP marker correct\\n"
quit 0
EOF
"$qemu" -M mps2-an505 -cpu cortex-m33 -nographic \
  -S -gdb "tcp::$port" -kernel "$tmp/handoff.elf" \
  >"$tmp/qemu.log" 2>&1 &
qemu_pid=$!
if ! timeout 20s "$gdb" -q -batch -x "$tmp/test.gdb"; then
    echo "[FAIL] QEMU handoff test; emulator log:" >&2
    cat "$tmp/qemu.log" >&2
    exit 1
fi
echo "[PASS] E62 Cortex-M33 CPU handoff harness (not product Boot ELF)"
