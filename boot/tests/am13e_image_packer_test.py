#!/usr/bin/env python3
"""End-to-end Python CLI contract checks for the E62 v2 flat image packer."""

import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

REPO = Path(__file__).resolve().parents[2]
PACKER = REPO / "boot" / "tools" / "pack_am13e_v2.py"
APP_BASE = 0x6000
IMAGE_SIZE = 5123


def run(*args, success=True):
    cp = subprocess.run([sys.executable, str(PACKER), *map(str, args)],
                        capture_output=True, text=True, check=False)
    if success and cp.returncode != 0:
        raise AssertionError(f"Unexpected failure: {cp.stdout}\n{cp.stderr}")
    if not success and cp.returncode == 0:
        raise AssertionError(f"Unexpected acceptance: {' '.join(map(str, args))}")
    return cp


def main():
    count = 0
    with tempfile.TemporaryDirectory(prefix="e62-pack-test-") as d:
        root = Path(d)
        raw_path = root / "raw.bin"
        packed_path = root / "packed.e62v2.bin"
        manifest_path = root / "packed.json"
        raw = bytearray(b"\xff" * IMAGE_SIZE)
        for offset in range(0x800, IMAGE_SIZE):
            raw[offset] = (offset * 13 + 7) & 0xFF
        # E62 v1.6 vectors begin at APP+0.
        struct.pack_into("<II", raw, 0, 0x20001000, APP_BASE + 0x900 + 1)
        raw_path.write_bytes(raw)
        run("pack", raw_path, packed_path, "--manifest", manifest_path)
        packed = packed_path.read_bytes()
        assert len(packed) == 5136 and packed[0x400:0x402] == b"\xea\x32"
        assert packed[0x500:0x504] == b"E62I"
        assert packed[IMAGE_SIZE:] == b"\xff" * (len(packed) - IMAGE_SIZE)
        assert packed[:8] == raw[:8]
        print("PASS v2 image packed with metadata/vector and 16-byte alignment")
        count += 1

        details = json.loads(run("verify", packed_path).stdout)
        manifest = json.loads(manifest_path.read_text())
        assert details == manifest and details["image_length"] == len(packed)
        assert details["vector_address"] == APP_BASE
        assert details["signature_address"] == APP_BASE + 0x400
        print("PASS independent pack / verify CLI and manifest roundtrip")
        count += 1

        expected_payload_crc = zlib.crc32(packed[:0x500] + packed[0x520:])
        assert struct.unpack_from("<I", packed, 0x510)[0] == expected_payload_crc
        assert struct.unpack_from("<I", packed, 0x51c)[0] == zlib.crc32(packed[0x500:0x51c])
        print("PASS v1 CRC-32/ISO-HDLC byte-span compatibility")
        count += 1

        corrupt = bytearray(packed)
        corrupt[0x1000] ^= 0x01
        (root / "corrupt.bin").write_bytes(corrupt)
        run("verify", root / "corrupt.bin", success=False)
        print("PASS corrupted application data rejected")
        count += 1

        corrupt = bytearray(packed)
        corrupt[0x51c] ^= 0x01
        (root / "header_bad.bin").write_bytes(corrupt)
        run("verify", root / "header_bad.bin", success=False)
        print("PASS corrupted metadata CRC rejected")
        count += 1

        (root / "truncated.bin").write_bytes(packed[:-16])
        run("verify", root / "truncated.bin", success=False)
        print("PASS truncated packed image rejected")
        count += 1

        legacy = bytearray(raw)
        legacy[:8] = b"\xff" * 8
        struct.pack_into("<II", legacy, 0x800, 0x20001000, APP_BASE + 0x901)
        legacy[:2] = b"\xea\x32"
        (root / "legacy.bin").write_bytes(legacy)
        run("pack", root / "legacy.bin", root / "legacy-out.bin", success=False)
        print("PASS old APP+0 signature / +0x800 vectors rejected")
        count += 1

        wrong_vector = bytearray(raw)
        struct.pack_into("<I", wrong_vector, 4, APP_BASE + 0xFFFF + 1)
        (root / "wrong-vector.bin").write_bytes(wrong_vector)
        run("pack", root / "wrong-vector.bin", root / "wrong-out.bin", success=False)
        print("PASS out-of-range M33 Reset Handler rejected")
        count += 1

        (root / "oversize.bin").write_bytes(b"\xff" * (256 * 1024 + 16))
        run("pack", root / "oversize.bin", root / "too-large.bin", success=False)
        print("PASS 256 KiB CMD_WRITE address limit enforced")
        count += 1

        (root / "nonempty-header.bin").write_bytes(raw[:0x500] + b"\x01" + raw[0x501:])
        run("pack", root / "nonempty-header.bin", root / "bad-header.bin", success=False)
        print("PASS occupied metadata window rejected")
        count += 1

        run("pack", raw_path, raw_path, success=False)
        print("PASS input overwrite blocked")
        count += 1

    print(f"PASS {count} E62 v2 image packer tests")


if __name__ == "__main__":
    main()
