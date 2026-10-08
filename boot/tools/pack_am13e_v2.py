#!/usr/bin/env python3
"""Pack/verify E62 AM13E v2 flat images; no ELF linker or hardware flashing.

Input is a raw binary whose first byte corresponds to APP_BASE=0x6000.
The Cortex-M33 vector table MUST be at APP+0x800, not APP+0 (v1).
Do not pass an old vector-first .e62.bin to this tool.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

APP_BASE = 0x6000
VECTOR_OFFSET = 0x800
HEADER_OFFSET = 0x100
HEADER_SIZE = 32
MAX_LENGTH = 256 * 1024
HEADER_MAGIC = 0x49323645  # "E62I"
TARGET_ID = 0x33314D41    # "AM13"
VERSION = 1
HEADER_FORMAT = "<IHHIIIIII"
assert struct.calcsize(HEADER_FORMAT) == HEADER_SIZE


class ImageError(ValueError):
    """A raw image violates the E62 v2 boot image contract."""


def _u32(data: bytes, at: int) -> int:
    return struct.unpack_from("<I", data, at)[0]


def _vectors(image: bytes) -> None:
    if len(image) < VECTOR_OFFSET + 16:
        raise ImageError("Image is too short to contain the M33 vectors at APP+0x800")
    sp = _u32(image, VECTOR_OFFSET)
    pc = _u32(image, VECTOR_OFFSET + 4)
    if sp < 0x20000008 or sp > 0x20018000 or (sp & 7):
        raise ImageError(f"Invalid M33 initial MSP at APP+0x800: {sp:#010x}")
    if (pc & 1) == 0 or not (APP_BASE + VECTOR_OFFSET <= (pc & ~1) < APP_BASE + len(image)):
        raise ImageError(f"Invalid M33 Reset Handler address: {pc:#010x}")


def _header_crc(metadata: bytes) -> int:
    return zlib.crc32(metadata[:28]) & 0xFFFFFFFF


def _payload_crc(image: bytes) -> int:
    return zlib.crc32(image[:HEADER_OFFSET] +
                      image[HEADER_OFFSET + HEADER_SIZE:]) & 0xFFFFFFFF


def _reject_v1_vectors(image: bytes) -> None:
    if len(image) < 8:
        return
    sp = _u32(image, 0)
    pc = _u32(image, 4)
    if 0x20000008 <= sp <= 0x20018000 and (sp & 7) == 0 and (pc & 1):
        raise ImageError("Vector-first E62 v1 image detected at APP+0; v2 expects APP+0x800")


def pack(raw: bytes) -> bytes:
    _reject_v1_vectors(raw)
    if len(raw) > MAX_LENGTH:
        raise ImageError("Image exceeds 256 KiB legacy CMD_WRITE addressing limit")
    if len(raw) < VECTOR_OFFSET + 16:
        raise ImageError("Raw flat binary does not include the APP+0x800 vector table")
    if raw[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE] not in (
        b"\\xff" * HEADER_SIZE, b"\\x00" * HEADER_SIZE
    ):
        raise ImageError("Image header slot at APP+0x100 must be uninitialized (all FF/00)")
    if raw[:2] not in (b"\\xff\\xff", b"\\xea\\x32"):
        raise ImageError("Image base must contain erased or ESCape32 signature bytes")
    image = bytearray(raw)
    image.extend(b"\\xff" * ((-len(image)) & 15))
    if len(image) > MAX_LENGTH:
        raise ImageError("16-byte padded image exceeds 256 KiB")
    image[0:2] = b"\\xea\\x32"
    _vectors(image)
    image[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE] = b"\\xff" * HEADER_SIZE
    crc = _payload_crc(image)
    first28 = struct.pack("<IHHIIIII", HEADER_MAGIC, VERSION, HEADER_SIZE,
                          TARGET_ID, len(image), crc, 0, 0)
    header = first28 + struct.pack("<I", _header_crc(first28))
    image[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE] = header
    result = bytes(image)
    verify(result)
    return result


def verify(image: bytes) -> dict[str, int | str]:
    if len(image) < VECTOR_OFFSET + 16 or len(image) > MAX_LENGTH or len(image) & 15:
        raise ImageError("Invalid image size or alignment")
    _reject_v1_vectors(image)
    if image[:2] != b"\\xea\\x32":
        raise ImageError("ESCape32 signature missing at APP+0")
    fields = struct.unpack_from(HEADER_FORMAT, image, HEADER_OFFSET)
    magic, version, size, target, length, image_crc, flags, reserved, hdr_crc = fields
    if (magic, version, size, target, flags, reserved) != (
        HEADER_MAGIC, VERSION, HEADER_SIZE, TARGET_ID, 0, 0
    ):
        raise ImageError("Incorrect E62 image header fields / target / flags")
    if hdr_crc != _header_crc(image[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE]):
        raise ImageError("E62 image header CRC mismatch")
    if length != len(image):
        raise ImageError(f"Declared length {length} differs from file length {len(image)}")
    _vectors(image)
    actual_crc = _payload_crc(image)
    if image_crc != actual_crc:
        raise ImageError(f"Payload CRC mismatch: expected {image_crc:08x}, got {actual_crc:08x}")
    return {
        "contract": "E62 AM13E v2 signature-last",
        "app_base": APP_BASE,
        "vector_address": APP_BASE + VECTOR_OFFSET,
        "header_address": APP_BASE + HEADER_OFFSET,
        "image_length": length,
        "image_crc32": f"{image_crc:08x}",
        "header_crc32": f"{hdr_crc:08x}",
        "sha256": hashlib.sha256(image).hexdigest(),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)
    pack_parser = sub.add_parser("pack", help="Create a CRC-protected E62 v2 image")
    pack_parser.add_argument("raw", type=Path)
    pack_parser.add_argument("output", type=Path)
    pack_parser.add_argument("--manifest", type=Path, default=None)
    verify_parser = sub.add_parser("verify", help="Verify an E62 v2 image file")
    verify_parser.add_argument("image", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.operation == "pack":
            if args.raw.resolve() == args.output.resolve():
                raise ImageError("Input and output must not be the same path")
            result = pack(args.raw.read_bytes())
            args.output.write_bytes(result)
            summary = verify(result)
            if args.manifest:
                args.manifest.write_text(json.dumps(summary, indent=2) + "\\n")
        else:
            summary = verify(args.image.read_bytes())
        print(json.dumps(summary, indent=2))
    except (ImageError, OSError) as exc:
        print(f"E62 v2 image {args.operation} FAILED: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
