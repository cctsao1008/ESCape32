#!/usr/bin/env python3
"""Rel17 v1.4 documentation/retired ABI synchronization gate.

This gate checks statements and source selection. It never claims
physical verification or completion of mandatory missing Boot commands.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]


def read(path):
    p = ROOT / path
    if not p.is_file():
        raise RuntimeError(f"Missing v1.4 document: {path}")
    return p.read_text(encoding="utf-8")


def require(condition, label):
    if not condition:
        raise RuntimeError(label)


def contains(path, *terms):
    content = read(path)
    for term in terms:
        require(term in content, f"{path}: missing required contract {term!r}")
    return content


def main():
    host = contains(
        "boot/tests/am13e_host/README.md",
        "Rel17 v1.4 (CURRENT)", "Cfg.id=0x32EA",
        "488 KiB maximum Flash allocation", "actual firmware image length",
        "CMD_WINDOW=6", "CMD_UPDATE", "CMD_SETWRP", "RES_ERROR",
        "Signature-last", "does not prove", "Hardware Validation PASS",
    )
    require("pack_am13e_v2.py pack" not in host,
            "Legacy v2 CLI described as current update path")
    require("Header CRC =" not in host and "Image CRC =" not in host,
            "Legacy header/image CRC fields presented as current spec")
    archive = read("boot/tests/am13e_host/README_V16_SUPERSEDED.md")
    require(archive.startswith("> **SUPERSEDED"),
            "Historical v1.6 host instructions not marked superseded")
    contains("mcu/AM13E/APP_LINK_CONTRACT.md",
             "488 KiB maximum", "actual firmware size",
             "Cfg.id=0x32EA", "CMD_WINDOW=6",
             "CMD_UPDATE", "CMD_SETWRP", "RES_ERROR")
    board = contains("mcu/AM13E/BOARD_CONFIGURATION.md",
                     "IO initialization", "Gate Driver Enable",
                     "Driver nFAULT", "Independent OC Trip",
                     "Serial Telemetry TX", "Current Limiting",
                     "Rel17 v1.4")
    require("Boot v1.6 image ABI" not in board and
            "existing v1.6 image integrity" not in board,
            "Board guide still claims superseded Image ABI is current")
    audit = contains("mcu/AM13E/REL17_V14_SOURCE_GAP_AUDIT.md",
                     "CMD_UPDATE", "CMD_SETWRP", "RES_ERROR",
                     "Conditional native adapter missing",
                     "488 KiB", "Cfg.id=0x32EA",
                     "Documentation / Specification Synchronization")
    require("PARTIAL" in audit and "Hardware" in audit,
            "Source Gap Audit must retain partial/hardware limits")
    for path in ("mcu/AM13E/PORTING_STATUS.md",
                 "mcu/AM13E/AM13E_FW1_BACKEND_PSEUDOCODE.md"):
        require("HISTORICAL" in read(path)[:500],
                f"Historical ledger missing non-normative notice: {path}")
    for path in ("boot/mcu/AM13E/image_integrity.c",
                 "boot/mcu/AM13E/image_integrity.h",
                 "mcu/AM13E/tools/verify_v16_image.py",
                 "mcu/AM13E/profiles/image_reference_v16.cmake",
                 "mcu/AM13E/linker_app_v16.ld"):
        require("SUPERSEDED" in read(path)[:300],
                f"Legacy artifact lacks SUPSERSEDED marker: {path}")
    boot = read("boot/tests/am13e_host/CMakeLists.txt")
    require("app_validity.c" in boot and
            "image_integrity.c" not in boot and
            "am13e_image_integrity_gate" not in boot,
            "Current Host tests must use v1.4 Cfg/vector validity")
    build = read("CMakeLists.txt")
    require('AM13E_IMAGE_PROFILE "REL17_V14"' in build and
            "image_rel17_v14.cmake" in build,
            "Active FW1 CMake image profile is not Rel17 v1.4")
    for path in ("boot/mcu/AM13E/app.c", "boot/mcu/AM13E/flash.c"):
        require("boot_am13e_image_check(" not in read(path),
                f"Old v1.6 whole-image gate reintroduced: {path}")
    print("PASS: Rel17 v1.4 current docs and 488KiB allocation semantics")
    print("PASS: legacy v1.6 instructions/ABI quarantined as SUPERSEDED")
    print("PENDING: CMD_UPDATE, CMD_SETWRP and conditional MCU backends")
    print("LIMIT: software/static documentation check, NOT hardware validation")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        sys.exit("FAIL v1.4 documentation synchronization: " + str(error))
