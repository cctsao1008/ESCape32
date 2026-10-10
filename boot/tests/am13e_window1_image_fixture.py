#!/usr/bin/env python3
"""Pad the ACTUAL linked flat Rel17 M33 program to test CMD_WINDOW upper
addressing; test fixture ONLY. No application metadata or CRC inserted.
"""
from pathlib import Path
import sys
if len(sys.argv)not in(3,4) or (len(sys.argv)==4 and sys.argv[3]!="--full"):
    raise SystemExit("usage: fixture.py REAL_FLAT OUTPUT [--full]")
source=Path(sys.argv[1]).read_bytes()
dest=Path(sys.argv[2])
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/"boot/tools"))
from pack_am13e_rel17 import pack,verify
target=(488 if len(sys.argv)==4 else 257)*1024
if len(source)<8 or len(source)>=target:
    raise SystemExit("Linked program must be smaller than test fixture region")
flat=pack(source+b"\xff"*(target-len(source)))
if len(flat)!=target or verify(flat)["image_length"]!=target:
    raise SystemExit("Flat CMD_WINDOW fixture length mismatch")
dest.write_bytes(flat)
print(f"PASS actual linked FW1 bytes with {target}-byte APP address-span fixture")
