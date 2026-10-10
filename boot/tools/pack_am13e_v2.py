#!/usr/bin/env python3
"""Pack/verify AM13E reference v1.6 vector-first APP image, no hardware flashing.

Input byte 0 is APP_BASE=0x6000 (Cortex-M33 vector table).
The signature-last marker is at APP+0x400, metadata APP+0x500.
Both are detailed-design reserved windows in the first 2KiB sector.
Reject legacy images with signature at APP+0 / vectors at APP+0x800.
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
VECTOR_OFFSET = 0x000
SIGNATURE_OFFSET = 0x400
HEADER_OFFSET = 0x500
METADATA_SECTOR = 0x800
HEADER_SIZE = 32
MAX_LENGTH = 256 * 1024
HEADER_MAGIC = 0x49323645  # "E62I"
TARGET_ID = 0x33314D41    # "AM13"
VERSION = 1
HEADER_FORMAT = "<IHHIIIIII"
assert struct.calcsize(HEADER_FORMAT) == HEADER_SIZE


class ImageError(ValueError):
    """A raw image violates the AM13E reference v2 boot image contract."""


def _u32(data: bytes, at: int) -> int:
    return struct.unpack_from("<I", data, at)[0]


def _vectors(image: bytes) -> None:
    if len(image) < METADATA_SECTOR + 16:
        raise ImageError("Image too short for vector-first, metadata and linked code")
    sp = _u32(image, VECTOR_OFFSET)
    pc = _u32(image, VECTOR_OFFSET + 4)
    if sp < 0x20000008 or sp > 0x20018000 or (sp & 7):
        raise ImageError(f"Invalid M33 initial MSP at APP+0: {sp:#010x}")
    if (pc & 1) == 0 or not (APP_BASE + METADATA_SECTOR <= (pc & ~1) < APP_BASE + len(image)):
        raise ImageError(f"Invalid M33 Reset Handler address: {pc:#010x}")


def _header_crc(metadata: bytes) -> int:
    return zlib.crc32(metadata[:28]) & 0xFFFFFFFF


def _payload_crc(image: bytes) -> int:
    return zlib.crc32(image[:HEADER_OFFSET] +
                      image[HEADER_OFFSET + HEADER_SIZE:]) & 0xFFFFFFFF


def _reject_old_boot_v2(image: bytes) -> None:
    if image[:2] == b"\xea\x32":
        raise ImageError("Old Boot-v2 APP+0 signature is incompatible with v1.6 vector-first")
    if len(image) >= 0x808:
        old_sp = _u32(image, 0x800)
        first_sp = _u32(image, 0) if len(image) >= 4 else 0
        if 0x20000008 <= old_sp <= 0x20018000 and not (
            0x20000008 <= first_sp <= 0x20018000):
            raise ImageError("Legacy APP+0x800 vector table detected")


def pack(raw: bytes) -> bytes:
    _reject_old_boot_v2(raw)
    if len(raw) > MAX_LENGTH:
        raise ImageError("Image exceeds 256 KiB legacy CMD_WRITE addressing limit")
    if len(raw) < METADATA_SECTOR + 2:
        raise ImageError("Raw binary has no linked code after first metadata sector")
    if raw[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE] not in (
        b"\xff" * HEADER_SIZE, b"\x00" * HEADER_SIZE
    ):
        raise ImageError("Image header slot at APP+0x100 must be uninitialized (all FF/00)")
    if raw[SIGNATURE_OFFSET:SIGNATURE_OFFSET + 16] not in (
        b"\xff" * 16, b"\x00" * 16
    ):
        raise ImageError("Signature ECC16 slot at APP+0x400 is not erased")
    image = bytearray(raw)
    image.extend(b"\xff" * ((-len(image)) & 15))
    if len(image) > MAX_LENGTH:
        raise ImageError("16-byte padded image exceeds 256 KiB")
    _vectors(image)
    image[SIGNATURE_OFFSET:SIGNATURE_OFFSET + 16] = b"\xff" * 16
    image[SIGNATURE_OFFSET:SIGNATURE_OFFSET + 2] = b"\xea\x32"
    image[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE] = b"\xff" * HEADER_SIZE
    crc = _payload_crc(image)
    first28 = struct.pack("<IHHIIIII", HEADER_MAGIC, VERSION, HEADER_SIZE,
                          TARGET_ID, len(image), crc, 0, 0)
    header = first28 + struct.pack("<I", _header_crc(first28))
    image[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE] = header
    result = bytes(image)
    verify(result)
    return result


def verify(image: bytes) -> dict[str, int | str]:
    if len(image) < METADATA_SECTOR + 16 or len(image) > MAX_LENGTH or len(image) & 15:
        raise ImageError("Invalid image size or alignment")
    _reject_old_boot_v2(image)
    if image[SIGNATURE_OFFSET:SIGNATURE_OFFSET + 2] != b"\xea\x32":
        raise ImageError("ESCape32 signature missing at APP+0x400")
    fields = struct.unpack_from(HEADER_FORMAT, image, HEADER_OFFSET)
    magic, version, size, target, length, image_crc, flags, reserved, hdr_crc = fields
    if (magic, version, size, target, flags, reserved) != (
        HEADER_MAGIC, VERSION, HEADER_SIZE, TARGET_ID, 0, 0
    ):
        raise ImageError("Incorrect AM13E image header fields / target / flags")
    if hdr_crc != _header_crc(image[HEADER_OFFSET:HEADER_OFFSET + HEADER_SIZE]):
        raise ImageError("AM13E image header CRC mismatch")
    if length != len(image):
        raise ImageError(f"Declared length {length} differs from file length {len(image)}")
    _vectors(image)
    actual_crc = _payload_crc(image)
    if image_crc != actual_crc:
        raise ImageError(f"Payload CRC mismatch: expected {image_crc:08x}, got {actual_crc:08x}")
    return {
        "contract": "E62 AM13E v1.6 vector-first signature-last",
        "app_base": APP_BASE,
        "vector_address": APP_BASE + VECTOR_OFFSET,
        "header_address": APP_BASE + HEADER_OFFSET,
        "signature_address": APP_BASE + SIGNATURE_OFFSET,
        "image_length": length,
        "image_crc32": f"{image_crc:08x}",
        "header_crc32": f"{hdr_crc:08x}",
        "sha256": hashlib.sha256(image).hexdigest(),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)
    pack_parser = sub.add_parser("pack", help="Create a CRC-protected AM13E reference v2 image")
    pack_parser.add_argument("raw", type=Path)
    pack_parser.add_argument("output", type=Path)
    pack_parser.add_argument("--manifest", type=Path, default=None)
    verify_parser = sub.add_parser("verify", help="Verify an AM13E reference v2 image file")
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
                args.manifest.write_text(json.dumps(summary, indent=2) + "\n")
        else:
            summary = verify(args.image.read_bytes())
        print(json.dumps(summary, indent=2))
    except (ImageError, OSError) as exc:
        print(f"AM13E reference v2 image {args.operation} FAILED: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
