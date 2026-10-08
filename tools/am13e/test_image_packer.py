#!/usr/bin/env python3
"""Hardware-free E62 image packer regression (reference format checks).

The validation function below independently checks the serialized contract.
It does NOT execute the target's boot_image.c and cannot establish HW PASS.
"""
from __future__ import annotations
import json
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "tools/am13e/pack-am13e-image.py"
SIZE = 0x7A000
HEADER = 0x100
MAGIC = 0x49323645
TARGET = 0x33314D41
FMT = "<IHHIIIIII"

def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xffffffff

def validate(image: bytes) -> bool:
    if len(image) < HEADER + 32:
        return False
    fields = struct.unpack_from(FMT, image, HEADER)
    magic, version, hlen, target, length, payload_crc, flags, reserved, hcrc = fields
    if not (magic == MAGIC and version == 1 and hlen == 32 and target == TARGET
            and 288 <= length <= SIZE and (length & 15) == 0
            and flags == 0 and reserved == 0 and length <= len(image)):
        return False
    if crc32(image[HEADER:HEADER + 28]) != hcrc:
        return False
    return crc32(image[:HEADER] + image[HEADER + 32:length]) == payload_crc

def main() -> None:
    with tempfile.TemporaryDirectory() as temp:
        folder = Path(temp)
        original = folder / "raw.bin"
        packed = folder / "packed.bin"
        manifest = folder / "packed.json"
        source = bytearray((i * 29 + 7) % 256 for i in range(4099))
        source[HEADER:HEADER + 32] = b"\x00" * 32
        original.write_bytes(source)
        command = [sys.executable, str(PACK), str(original), str(packed),
                   "--manifest", str(manifest)]
        subprocess.run(command, check=True, capture_output=True, text=True)
        image = packed.read_bytes()
        assert validate(image)
        assert len(image) == 4112
        meta = json.loads(manifest.read_text())
        assert meta["image_length"] == len(image)
        assert meta["image_crc32"] == f"0x{struct.unpack_from('<I', image, HEADER + 16)[0]:08X}"
        print("[PASS] packed image / header / payload CRC / manifest")

        saved = image
        subprocess.run(command, check=True, capture_output=True, text=True)
        assert packed.read_bytes() == saved
        print("[PASS] deterministic repacking")

        for name, index in (("payload corruption", 350),
                            ("header corruption", HEADER + 12),
                            ("vector corruption", 4)):
            broken = bytearray(saved)
            broken[index] ^= 0x01
            assert not validate(broken), name
        assert not validate(saved[:HEADER + 25])
        assert not validate(saved[:1000])
        print("[PASS] corrupted payload / header / vector / truncated image rejection")

        original.write_bytes(b"\xAA" * (SIZE + 1))
        oversize = subprocess.run(command, capture_output=True, text=True)
        assert oversize.returncode != 0
        print("[PASS] oversized image rejected")

    print("[PASS] E62 image packer host regression")

if __name__ == "__main__":
    main()
