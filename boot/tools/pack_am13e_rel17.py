#!/usr/bin/env python3
"""Prepare/check a FLAT ESCape32 Rel17 v1.4 AM13E M33 application binary.

APP_BASE=0x6000. Actual image bytes are the linked ELF/objcopy output,
not a 488KiB fixed container. Only a 4-byte transport alignment pad
(all FF, at most 3 bytes) may be appended for original recvdata framing.
No image header, APP signature, embedded CRC, or boot validity envelope.
Boot launch uses Cfg.id=0x32EA at 0x4000 and plausible M33 vectors.
The 488KiB flash allocation is an upper bound, not an image length.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

APP_BASE=0x6000
APP_END=0x80000
CFG_ID_ADDR=0x4000
MAX_LENGTH=APP_END-APP_BASE
TRANSPORT_ALIGN=4

class ImageError(ValueError):
    """Invalid flat Rel17 AM13E application image."""

def _vectors(image:bytes)->tuple[int,int]:
    if len(image)<8:
        raise ImageError("M33 APP requires initial SP and Reset PC")
    sp,pc=struct.unpack_from("<II",image,0)
    if sp<0x20000008 or sp>0x20018000 or (sp&7):
        raise ImageError(f"Invalid M33 stack pointer {sp:#x}")
    entry=pc&~1
    if not(pc&1) or not(APP_BASE<=entry<APP_BASE+len(image)):
        raise ImageError(f"Invalid linked Thumb Reset PC {pc:#x}")
    return sp,pc

def pack(raw:bytes)->bytes:
    if len(raw)>MAX_LENGTH:
        raise ImageError("Linked firmware exceeds 488KiB APP allocation")
    _vectors(raw)
    pad=(-len(raw))%TRANSPORT_ALIGN
    if len(raw)+pad>MAX_LENGTH:
        raise ImageError("4-byte transport padding exceeds APP allocation")
    return raw+b"\xff"*pad

def verify(image:bytes)->dict[str,int|str]:
    if len(image)>MAX_LENGTH or len(image)%TRANSPORT_ALIGN:
        raise ImageError("Transport image exceeds APP or is not 4-byte-aligned")
    sp,pc=_vectors(image)
    return {
        "contract":"Rel17 v1.4 flat APP / Cfg.id boot gate",
        "app_base":APP_BASE,
        "cfg_marker_address":CFG_ID_ADDR,
        "app_allocation_max_bytes":MAX_LENGTH,
        "image_length":len(image),
        "initial_sp":f"{sp:08x}",
        "reset_pc":f"{pc:08x}",
        "sha256":hashlib.sha256(image).hexdigest(),
    }

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest="op",required=True)
    pk=sub.add_parser("pack",help="Copy and validate linked flat ELF-derived BIN")
    pk.add_argument("raw",type=Path)
    pk.add_argument("output",type=Path)
    pk.add_argument("--manifest",type=Path)
    vr=sub.add_parser("verify",help="Check flat transport bounds and M33 vectors")
    vr.add_argument("image",type=Path)
    a=p.parse_args()
    try:
        if a.op=="pack":
            if a.raw.resolve()==a.output.resolve():
                raise ImageError("Source and output paths must differ")
            raw=a.raw.read_bytes()
            image=pack(raw)
            a.output.write_bytes(image)
            meta=verify(image)
            meta["linked_raw_bytes"]=len(raw)
            meta["padding_bytes"]=len(image)-len(raw)
            if a.manifest:
                a.manifest.write_text(json.dumps(meta,indent=2)+"\n")
        else:meta=verify(a.image.read_bytes())
        print(json.dumps(meta,indent=2))
        return 0
    except (ImageError,OSError) as exc:
        print("Rel17 v1.4 flat APP "+a.op+" failed: "+str(exc),file=sys.stderr)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
