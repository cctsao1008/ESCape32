#!/usr/bin/env python3
"""Pack an AM13E23019 application binary with the E62 image header."""

from __future__ import annotations

import argparse
import json
import struct
import zlib
from pathlib import Path

APP_SIZE = 0x0007A000
HEADER_OFFSET = 0x100
HEADER_SIZE = 0x20
HEADER_MAGIC = 0x49323645  # "E62I"
HEADER_VERSION = 1
TARGET_ID = 0x33314D41     # "AM13"
FLAGS = 0


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_bin", type=Path)
    parser.add_argument("output_bin", type=Path)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()

    image = bytearray(args.input_bin.read_bytes())

    minimum = HEADER_OFFSET + HEADER_SIZE
    if len(image) < minimum:
        image.extend(b"\xFF" * (minimum - len(image)))

    if len(image) > APP_SIZE:
        raise SystemExit(
            f"image too large: {len(image)} bytes > APP region {APP_SIZE} bytes"
        )

    aligned_length = (len(image) + 15) & ~15
    image.extend(b"\xFF" * (aligned_length - len(image)))

    # The linked ELF reserves this window with erased words. Always normalize
    # it before computing the payload CRC so repacking is deterministic.
    image[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE] = b"\xFF" * HEADER_SIZE

    payload = (
        bytes(image[:HEADER_OFFSET])
        + bytes(image[HEADER_OFFSET + HEADER_SIZE:aligned_length])
    )
    image_crc = crc32(payload)

    header_without_crc = struct.pack(
        "<IHHIIIII",
        HEADER_MAGIC,
        HEADER_VERSION,
        HEADER_SIZE,
        TARGET_ID,
        aligned_length,
        image_crc,
        FLAGS,
        0,
    )
    assert len(header_without_crc) == HEADER_SIZE - 4

    header_crc = crc32(header_without_crc)
    header = header_without_crc + struct.pack("<I", header_crc)
    assert len(header) == HEADER_SIZE

    image[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE] = header

    args.output_bin.parent.mkdir(parents=True, exist_ok=True)
    args.output_bin.write_bytes(image)

    manifest = {
        "format": "E62-AM13E23019-image-v1",
        "app_base": "0x00006000",
        "image_length": aligned_length,
        "image_crc32": f"0x{image_crc:08X}",
        "header_offset": f"0x{HEADER_OFFSET:08X}",
        "header_crc32": f"0x{header_crc:08X}",
        "target_id": f"0x{TARGET_ID:08X}",
    }

    if args.manifest:
        args.manifest.write_text(
            json.dumps(manifest, indent=2) + "\n",
            encoding="utf-8",
        )

    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
