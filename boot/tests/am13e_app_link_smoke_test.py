#!/usr/bin/env python3
"""Assert ARM ELF -> objcopy BIN -> AM13E reference v2 packed image address contracts.

This verifies only a deliberately minimal Cortex-M33 linker smoke ELF,
not ESCape32 rel17 firmware or real AM13E hardware execution.
"""
from __future__ import annotations

import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib


def check(cond: bool, description: str) -> None:
    if not cond:
        raise AssertionError(description)
    print("PASS", description)


def sections(objdump: Path, elf: Path) -> dict[str, tuple[int, int]]:
    result = subprocess.run(
        [str(objdump), "-h", str(elf)],
        capture_output=True, text=True, check=True)
    found: dict[str, tuple[int, int]] = {}
    for line in result.stdout.splitlines():
        match = re.match(
            r"^\s*\d+\s+(\.[\w.]+)\s+([0-9a-fA-F]+)\s+"
            r"([0-9a-fA-F]+)\s+", line)
        if match:
            found[match[1]] = (int(match[3], 16), int(match[2], 16))
    return found


def main() -> int:
    if len(sys.argv) != 6:
        print("Usage: app_link_test.py RAW PACKED MANIFEST OBJDUMP ELF",
              file=sys.stderr)
        return 2
    raw, packed, manifest, objdump, elf = map(Path, sys.argv[1:])
    raw_bytes = raw.read_bytes()
    image = packed.read_bytes()
    info = json.loads(manifest.read_text())
    layout = sections(objdump, elf)

    check(layout.get(".intvecs", (None, 0))[0] == 0x6000 and
          layout[".intvecs"][1] >= 16,
          "ELF v1.6 M33 vector table starts at APP_BASE 0x6000")
    check(layout.get(".signature") == (0x6400, 16),
          "ELF ECC16 erased marker slot located at APP+0x400")
    check(layout.get(".image_header") == (0x6500, 32),
          "ELF 32-byte CRC metadata reservation at APP+0x500")
    check(layout.get(".text", (0, 0))[0] >= 0x6800 and
          layout[".text"][1] > 0,
          "ELF contains ARM application code after the M33 vectors")

    check(raw_bytes[0x400:0x410] == b"\xff" * 16 and
          raw_bytes[0x500:0x520] == b"\xff" * 32,
          "objcopy --gap-fill=0xff preserves erased metadata")
    check(len(raw_bytes) >= 0x802 and
          raw_bytes[0x600:0x800] == b"\xff" * 0x200,
          "raw BIN begins at 0x6000 and preserves metadata/vector gap")

    sp, pc = struct.unpack_from("<II", raw_bytes, 0)
    check(sp == 0x20018000 and pc & 1 and
          0x6800 <= pc & ~1 < 0x6000 + len(raw_bytes),
          "ARM-linked vector contains valid MSP and Thumb Reset_Handler")
    check(image[0x400:0x402] == b"\xea\x32" and
          image[:0x400] == raw_bytes[:0x400] and
          image[0x800:len(raw_bytes)] == raw_bytes[0x800:],
          "packer publishes signature without altering linked vector/code")
    check(info["image_length"] == len(image) and
          len(image) <= 256 * 1024 and len(image) % 16 == 0,
          "manifest length matches 16-byte-aligned transport image")

    header = image[0x500:0x520]
    check(zlib.crc32(header[:28]) ==
          struct.unpack_from("<I", header, 28)[0],
          "packed AM13E reference header CRC matches Python-independent CRC computation")
    check(zlib.crc32(image[:0x500] + image[0x520:]) ==
          struct.unpack_from("<I", header, 16)[0],
          "packed image CRC covers signed prefix and linker-generated payload")

    print("PASS AM13E reference v2 linked ARM ELF/BIN/packer contract smoke")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, subprocess.CalledProcessError) as exc:
        print(f"FAIL AM13E reference v2 ARM link smoke: {exc}", file=sys.stderr)
        raise SystemExit(1)
