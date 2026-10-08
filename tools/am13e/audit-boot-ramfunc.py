#!/usr/bin/env python3
"""Inspect AM13E Boot ELF same-bank Flash call targets and RAMFUNC placement.

Conservative STATIC audit, not proof of absence of Flash reads:
indirect branches, literal data loads, veneers, and controller busy windows
require manual disassembly review / on-target validation.
"""
import argparse
import re
import subprocess
from pathlib import Path

RAM_LO, RAM_HI = 0x00C18000, 0x00C20000
FUNCTIONS = (
    "boot_flash_bank0_erase_sector",
    "boot_flash_bank0_program",
    "DL_FlashCTL_executeCommand",
)
HEAD = re.compile(r"^([0-9a-fA-F]+) <([^>]+)>:$")
INSN = re.compile(r"^\s*([0-9a-fA-F]+):\s+(?:[0-9a-fA-F]{2,8}\s+)+(.+)$")
DIRECT = re.compile(r"\b(b(?:l|lx|\.w)?|blx)\s+(?:0x)?([0-9a-fA-F]+)\s*<([^>]+)>")
INDIRECT = re.compile(r"\b(?:blx|bx)\s+(?:r\d+|ip|lr)\b")
MEMORY = re.compile(r"\b(?:ldr|ldrb|ldrh|ldrsb|ldrsh|ldrd|tbb|tbh)\b", re.I)

def run(command):
    return subprocess.check_output(command, text=True)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", type=Path, required=True)
    ap.add_argument("--toolchain-bin", type=Path, required=True)
    args = ap.parse_args()
    nm = run([str(args.toolchain_bin / "arm-none-eabi-nm"), "-n", str(args.elf)])
    dis = run([str(args.toolchain_bin / "arm-none-eabi-objdump"), "-d", "-C", str(args.elf)])
    sections = run([str(args.toolchain_bin / "arm-none-eabi-objdump"), "-h", str(args.elf)])
    names = {}
    for line in nm.splitlines():
        fields = line.split()
        if len(fields) >= 3 and re.fullmatch(r"[0-9a-fA-F]+", fields[0]):
            names[fields[2]] = int(fields[0], 16)
    failures = []
    for name in FUNCTIONS:
        addr = names.get(name)
        if addr is None or not RAM_LO <= addr < RAM_HI:
            failures.append(f"{name}: missing or not in RAM_C ({addr})")
        else:
            print(f"[PASS] RAM_C symbol {name} @ 0x{addr:08X}")

    match = re.search(r"^\s*\d+\s+\.TI\.ramfunc\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)", sections, re.M)
    if not match:
        failures.append("missing .TI.ramfunc section")
    else:
        sz, start = (int(match.group(i), 16) for i in (1, 2))
        if not RAM_LO <= start < RAM_HI or start + sz > RAM_HI:
            failures.append(f"RAMFUNC section out of RAM_C: {start:#x} + {sz:#x}")
        else:
            print(f"[PASS] .TI.ramfunc VMA 0x{start:08X}, size {sz} bytes")
    current = None
    direct_out = []
    indirect = []
    loads = []
    for line in dis.splitlines():
        m = HEAD.match(line)
        if m:
            current = m.group(2)
            continue
        m = INSN.match(line)
        if not m:
            continue
        pc = int(m.group(1), 16)
        if not RAM_LO <= pc < RAM_HI:
            continue
        operation = m.group(2)
        d = DIRECT.search(operation)
        if d:
            dest, target_name = int(d.group(2), 16), d.group(3)
            if not RAM_LO <= dest < RAM_HI:
                direct_out.append((pc, current, dest, target_name, operation.strip()))
        if INDIRECT.search(operation):
            indirect.append((pc, current, operation.strip()))
        if MEMORY.search(operation):
            loads.append((pc, current, operation.strip()))

    print(f"RAM_C direct branch/call outside RAM_C: {len(direct_out)}")
    for pc, fn, dest, target, ins in direct_out:
        print(f"  0x{pc:08X} {fn}: {ins} -> 0x{dest:08X} {target}")
    print(f"RAM_C indirect branch candidates: {len(indirect)}")
    for pc, fn, ins in indirect[:30]:
        print(f"  0x{pc:08X} {fn}: {ins}")
    print(f"RAM_C memory-load instructions (manual literal review): {len(loads)}")
    for pc, fn, ins in loads[:25]:
        print(f"  0x{pc:08X} {fn}: {ins}")
    if len(loads) > 25:
        print("  ... remaining loads omitted; inspect objdump output manually")
    if direct_out:
        failures.append("RAM_C includes direct branches/calls outside RAM_C; review busy-time reachability")
    if failures:
        for reason in failures:
            print("[REVIEW] " + reason)
        raise SystemExit(1)
    print("[PASS] basic RAMFUNC placement and direct branch target static audit")
    print("[NOTE] indirect control flow / constants / Flash busy timing NOT established by this test")

if __name__ == "__main__":
    main()
