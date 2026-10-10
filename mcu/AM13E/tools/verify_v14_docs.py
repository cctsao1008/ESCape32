#!/usr/bin/env python3
"""Rev1.4 source-authority and retired-rule consistency checks.

Tests documentation/source contracts only; cannot assert that all
conditional MCU backends or hardware are implemented.
"""
from pathlib import Path
import hashlib
import json
import sys

ROOT=Path(__file__).resolve().parents[3]
DIGESTS={
    "Integration_Design.md":"06acbc4960e09d5c2ebe266e0961a5a1847a0748cd3a33f544865fb5040943f5",
    "Integration_Mapping.md":"ad833a9182d7766d59ce51c92a22c8efadff5dde581b16f7cb20eee3b37d2039",
    "Interface_Contracts.md":"c7d8d2744806d8bf138d721a9b6f373661b780be5381c8eac8038132f4c4743c",
}

def require(ok,why):
    if not ok: raise RuntimeError(why)

def read(path):
    file=ROOT/path
    require(file.is_file(),"Required v1.4 repository file absent: "+path)
    return file.read_text(encoding="utf-8")

def check_current_contract():
    authority=read("mcu/AM13E/V14_DESIGN_AUTHORITY.md")
    for name,digest in DIGESTS.items():
        require(name in authority and digest in authority,
                "Original v1.4 source provenance changed: "+name)
        # The user-supplied original files are mandatory and byte-identical.
        spec=ROOT/"mcu/AM13E/v1.4"/name
        require(spec.is_file(),"Canonical Rev1.4 source absent: "+name)
        require(hashlib.sha256(spec.read_bytes()).hexdigest()==digest,
                "Original Rev1.4 source checksum mismatch: "+name)
    for word in ("26 proposed private function names","CMD_UPDATE",
                 "CMD_SETWRP","CMD_WINDOW=6","Cfg.id=0x32EA",
                 "488 KiB","linked binary","source-defined/conditional"):
        require(word in authority,"Rev1.4 authority lost: "+word)

    retired=[
        "AM13E_ESCape32_Porting_Architecture_Rules.md",
        "mcu/AM13E/SOURCE_NAMING_POLICY.md",
        "mcu/AM13E/INTEGRATION_ARCHITECTURE_ALIGNMENT.md",
        "mcu/AM13E/FW1_PB14_ONLY_SCOPE.md",
        "mcu/AM13E/BOARD_CONFIGURATION.md",
        "mcu/AM13E/GENERIC_PLATFORM_SCOPE.md",
        "mcu/AM13E/tools/check_source_filenames.py",
    ]
    for path in retired:
        require(not (ROOT/path).exists(),
                "Superseded non-v1.4 rule still active: "+path)

    audit=read("mcu/AM13E/REL17_V14_SOURCE_GAP_AUDIT.md")
    require("Conditional native adapter missing" in audit and
            "CMD_UPDATE" in audit and "CMD_SETWRP" in audit and
            "NOT excluded" in audit,
            "Must track mandatory original Rel17 feature gaps")
    manifest=json.loads(read("mcu/AM13E/REL17_FUNCTIONAL_COVERAGE.json"))
    require("io_only" not in manifest and
            "reference_io_state" in manifest and
            "conditional_upstream_pending" in manifest and
            len(manifest["conditional_upstream_pending"])>0,
            "Legacy five-feature exclusion policy still in feature audit")

    host=read("boot/tests/am13e_host/README.md")
    require("Cfg.id=0x32EA" in host and
            "488 KiB maximum Flash allocation" in host and
            "Signature-last" in host,
            "Current Rel17 v1.4 Boot/Image ABI has drifted")
    legacy=read("boot/tests/am13e_host/README_V16_SUPERSEDED.md")
    require(legacy.startswith("> **SUPERSEDED"),
            "Historical v1.6 test instructions not quarantined")
    board=read("mcu/AM13E/REFERENCE_BOARD_STATUS.md")
    require("NOT a board policy" in board and
            "no claimed power-stage/hardware qualification" in board,
            "Current Reference IO state cannot become a porting exclusion")
    conditional=read("mcu/AM13E/CONDITIONAL_FEATURE_GAPS.md")
    for feature in ("Analog receiver","Serial/iBUS","Hall/hybrid",
                    "CMD_UPDATE","CMD_SETWRP"):
        require(feature in conditional,"Original conditional feature missing: "+feature)
    cfg=read("mcu/AM13E/APP_LINK_CONTRACT.md")
    require("Actual firmware size" in cfg and
            "488 KiB maximum" in cfg and
            "CMD_WINDOW=6" in cfg,
            "Rev1.4 linked-size/transport semantics lost")

    build=read("CMakeLists.txt")
    boot_cmake=read("boot/CMakeLists.txt")
    require("function(add_target" in build and
            "add_target(BOOT5_PB14 AM13E" in boot_cmake,
            "Native ESCape32 Application/Boot target pattern absent")
    script=read(".github/workflows/am13e-fw1-compile-link.yml")
    require("check_source_filenames.py" not in script,
            "Superseded naming checker still runs in CI")
    require("-Wl,--require-defined=write" in read("CMakeLists.txt") and
            "-Wl,--require-defined=boot_am13e_flash_write" not in
            read("CMakeLists.txt"),
            "ARM Boot linker still requires legacy private Flash symbol")
    require("int write(char *dst" in read("boot/src/common.h") and
            "sendval(write(write_addr, buf, len)" in
            read("boot/src/main.c"),
            "Original Boot write() hook must be reused in AM13E parser")
    flash=read("boot/mcu/AM13E/flash.c")
    require("#define AM13E_WRITE_ENTRY write" in flash and
            "int AM13E_WRITE_ENTRY(" in flash,
            "AM13E must export original Boot write(), not a parallel public API")
    require("Original Rel17 Boot write()" in
            read("mcu/AM13E/V14_INTERFACE_COMPLIANCE_AUDIT.md"),
            "Rev1.4 Interface audit missing restored Boot write()")

    print("PASS: exact original Rev1.4 source docs SHA-256")
    print("PASS: original Rel17 Boot write() hook restored in native ARM source")
    print("PASS: superseded naming/HAL/PB14-only/five-feature exclusion rules removed")
    print("PASS: missing original Boot and conditional MCU features still tracked")
    print("SCOPE: source/document evidence only; full feature parity/hardware pending")

if __name__=="__main__":
    try: check_current_contract()
    except (OSError,RuntimeError,ValueError,KeyError) as exc:
        sys.exit("FAIL Rev1.4 specification authority: "+str(exc))
