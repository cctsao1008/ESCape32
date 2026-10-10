#!/usr/bin/env python3
"""Create a CRC-valid 257KiB v1.6 ARM FW1 image from an ACTUAL linked
packed image, for CMD_WINDOW=6 host end-to-end protocol regression.

No mock firmware reset vector: the prefix and every code byte come from
the ARM-linked firmware, only erased padding extends its image length.
"""
from pathlib import Path
import sys

if len(sys.argv)!=3:
    raise SystemExit("usage: fixture.py ARM_LINKED_PACKED_BIN OUTPUT_PACKED_BIN")
real=Path(sys.argv[1]).read_bytes()
out=Path(sys.argv[2])
repo=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(repo/"boot/tools"))
from pack_am13e_v2 import pack,verify,HEADER_OFFSET,HEADER_SIZE,SIGNATURE_OFFSET

need=257*1024
if len(real)<0x900 or len(real)>=need:
    raise SystemExit("Expected actual ARM-linked image smaller than 257KiB")
raw=bytearray(real)
raw[SIGNATURE_OFFSET:SIGNATURE_OFFSET+16]=b"\xff"*16
raw[HEADER_OFFSET:HEADER_OFFSET+HEADER_SIZE]=b"\xff"*HEADER_SIZE
raw.extend(b"\xff"*(need-len(raw)))
p=pack(bytes(raw))
if verify(p)["image_length"]!=need:
    raise SystemExit("Invalid window1 image metadata")
out.write_bytes(p)
print(f"PASS: real linked FW1 prefix extended into CMD_WINDOW=1 ({need} bytes)")
