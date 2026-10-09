#!/usr/bin/env python3
"""Static ELF audit of the AM13E E62 Boot Flash-service RAM execution path.

Checks ELF VMA/LMA, the startup RAM-copy linker symbols, required DriverLib
function placement and direct branches from .TI.ramfunc.

This is NOT a hardware test and does not prove indirect branches, literal/data
loads, Flash ECC behavior, interrupt execution or startup copy at runtime.
"""
import argparse
import pathlib
import re
import shutil
import subprocess
import sys

REQUIRED_FUNCTIONS = (
    "boot_am13e_flash_execute",
    "DL_Flash_eraseSector",
    "DL_Flash_program",
    "DL_FlashCTL_eraseMemory",
    "DL_FlashCTL_unprotectSector",
    "DL_FlashCTL_programMemory128WithECCGenerated",
    "DL_FlashCTL_readVerify128WithECCGenerated",
    "DL_FlashCTL_programMemory64WithECCGenerated",
    "DL_FlashCTL_readVerify64WithECCGenerated",
    "DL_FlashCTL_blankVerify",
    "DL_FlashCTL_executeCommand",
)
COPY_SYMBOLS = ("__ramfunct_load__", "__ramfunct_start__", "__ramfunct_end__")


def run(command, *args):
    if shutil.which(command) is None:
        raise RuntimeError("Tool not available: " + command)
    result = subprocess.run(
        (command,) + args, capture_output=True, text=True, check=False
    )
    if result.returncode != 0:
        raise RuntimeError(
            "{} returned {}: {}".format(
                command, result.returncode, result.stderr.strip()
            )
        )
    return result.stdout


def integer(value):
    return int(value, 0)


def audit(args):
    if not args.elf.is_file():
        raise RuntimeError("ELF not found: " + str(args.elf))

    hdr = run(args.prefix + "objdump", "-h", str(args.elf))
    nm = run(args.prefix + "nm", "-an", str(args.elf))
    dis = run(args.prefix + "objdump", "-d", "-j", ".TI.ramfunc", str(args.elf))

    notes, failures = [], []
    sections = {}
    section_re = re.compile(
        r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+"
        r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)"
    )
    for line in hdr.splitlines():
        match = section_re.match(line)
        if match:
            sections[match[1]] = {
                "size": int(match[2], 16),
                "vma": int(match[3], 16),
                "lma": int(match[4], 16),
            }
    ram = sections.get(".TI.ramfunc")
    ram_low, ram_high = args.ram_start, args.ram_start + args.ram_size
    flash_low = args.boot_flash_start
    flash_high = args.boot_flash_start + args.boot_flash_size

    if ram is None or ram["size"] == 0:
        failures.append("Missing or empty .TI.ramfunc")
    else:
        notes.append(
            ".TI.ramfunc VMA=0x{:08x} LMA=0x{:08x} size={} bytes".format(
                ram["vma"], ram["lma"], ram["size"]
            )
        )
        if not (ram_low <= ram["vma"] < ram["vma"] + ram["size"] <= ram_high):
            failures.append(".TI.ramfunc run address is outside RAM_C")
        if not (
            flash_low <= ram["lma"] < ram["lma"] + ram["size"] <= flash_high
        ):
            failures.append(".TI.ramfunc load image is outside Boot Flash")

    symbols = {}
    symbol_re = re.compile(r"^([0-9a-fA-F]+)\s+([A-Za-z])\s+(\S+)")
    for line in nm.splitlines():
        match = symbol_re.match(line)
        if match:
            symbols.setdefault(match[3], []).append(
                (int(match[1], 16), match[2])
            )

    for name in COPY_SYMBOLS:
        if name not in symbols:
            failures.append("Startup copy symbol missing: " + name)
    if ram is not None and all(name in symbols for name in COPY_SYMBOLS):
        load, start, end = [
            symbols[name][0][0] for name in COPY_SYMBOLS
        ]
        notes.append(
            "Copy symbols load=0x{:08x} start=0x{:08x} end=0x{:08x}".format(
                load, start, end
            )
        )
        if (load, start, end) != (
            ram["lma"], ram["vma"], ram["vma"] + ram["size"]
        ):
            failures.append("Startup copy symbols disagree with RAM VMA/LMA")

    for function in REQUIRED_FUNCTIONS:
        matches = [
            (name, address)
            for name, locations in symbols.items()
            if name == function or name.startswith(function + ".")
            for address, _type in locations
        ]
        if not matches:
            failures.append("Required symbol not found: " + function)
        for name, address in matches:
            notes.append("{} = 0x{:08x}".format(name, address))
            if not (
                ram is not None
                and ram["vma"] <= address < ram["vma"] + ram["size"]
            ):
                failures.append(
                    "Required function not in RAM: {} at 0x{:08x}".format(
                        name, address
                    )
                )

    # objdump ARM/Thumb direct branch lines, including conditional branches.
    # This deliberately excludes BX/BLX through a register: indirect jumps
    # remain INCONCLUSIVE and must be reviewed separately.
    branch_re = re.compile(
        r"\b("
        r"blx?|b(?:\.w|\.n|eq|ne|cs|cc|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)"
        r"(?:\.w|\.n)?"
        r")\s+(?:0x)?([0-9a-fA-F]+)\s*<([^>]*)>", re.I
    )
    branches = 0
    for line in dis.splitlines():
        match = branch_re.search(line)
        if match is None:
            continue
        address = int(match[2], 16)
        branches += 1
        if ram is None or not (
            ram["vma"] <= address < ram["vma"] + ram["size"]
        ):
            failures.append("Direct RAM branch leaves RAM: " + line.strip())
    notes.append("Direct branch targets inspected: {}".format(branches))
    if branches == 0:
        failures.append("No direct branches found; disassembly check inconclusive")

    if args.link_map is not None:
        if not args.link_map.is_file():
            failures.append("Map file not found: " + str(args.link_map))
        else:
            map_text = args.link_map.read_text(errors="replace")
            for obj in ("dl_flash.c.obj", "dl_flashctl.c.obj"):
                if obj not in map_text:
                    failures.append("Map file does not include: " + obj)
            notes.append(
                "Map object names found; manually inspect .TI.ramfunc inputs"
            )

    print("=== E62 / AM13E BOOT RAM EXECUTION ELF AUDIT ===")
    for note in notes:
        print("[INFO]", note)
    for fail in failures:
        print("[FAIL]", fail)

    if failures:
        print("RESULT: FAIL / INCONCLUSIVE")
        return 1
    print("RESULT: STATIC SECTION / SYMBOL / DIRECT-BRANCH CHECK PASS")
    print(
        "LIMIT: indirect calls, const-data accesses, runtime startup copy, "
        "ECC and actual Flash hardware remain unverified."
    )
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=pathlib.Path)
    parser.add_argument("--map", dest="link_map", type=pathlib.Path)
    parser.add_argument("--prefix", default="arm-none-eabi-")
    parser.add_argument("--ram-start", type=integer, default=0x00C18000)
    parser.add_argument("--ram-size", type=integer, default=0x8000)
    parser.add_argument("--boot-flash-start", type=integer, default=0)
    parser.add_argument("--boot-flash-size", type=integer, default=0x4000)
    args = parser.parse_args()
    return audit(args)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as exc:
        print("[ERROR]", exc, file=sys.stderr)
        sys.exit(2)
