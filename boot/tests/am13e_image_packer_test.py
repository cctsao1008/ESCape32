#!/usr/bin/env python3
"""Rel17 v1.4 flat app packer: no embedded CRC/signature or fixed size."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
PACK=ROOT/"boot/tools/pack_am13e_rel17.py"
APP=0x6000
MAX=488*1024
def run(*args,success=True):
    p=subprocess.run([sys.executable,str(PACK),*map(str,args)],
                     text=True,capture_output=True)
    if success and p.returncode:
        raise AssertionError(p.stderr)
    if not success and not p.returncode:
        raise AssertionError("Unexpectedly accepted: "+str(args))
    return p
def vector(data,pc=None):
    struct.pack_into("<II",data,0,0x20001000,
                     pc if pc is not None else APP+0x101)
def main():
    with tempfile.TemporaryDirectory() as temp:
        d=Path(temp)
        raw=bytearray((i*13+7)&255 for i in range(5123))
        vector(raw)
        raw[0x400:0x410]=bytes(range(16))
        raw[0x500:0x520]=bytes(range(32))
        src=d/"raw.bin";out=d/"out.flat.bin";meta=d/"sidecar.json"
        src.write_bytes(raw)
        run("pack",src,out,"--manifest",meta)
        packed=out.read_bytes()
        assert packed[:len(raw)]==raw
        assert packed[len(raw):]==b"\xff"
        assert len(packed)==5124
        info=json.loads(meta.read_text())
        assert info["linked_raw_bytes"]==len(raw)
        assert info["image_length"]==len(packed)
        assert info["app_allocation_max_bytes"]==MAX
        assert info["cfg_marker_address"]==0x4000
        assert info["padding_bytes"]==1
        assert "image_crc32" not in info and "signature_address" not in info
        print("PASS unmodified ELF bytes including old +0x400/+0x500 offsets")
        assert json.loads(run("verify",out).stdout)["image_length"]==5124

        altered=bytearray(packed)
        altered[0x800]^=1
        (d/"altered.bin").write_bytes(altered)
        run("verify",d/"altered.bin") # v1.4 has NO image data CRC.
        print("PASS v1.4 intentionally has no payload CRC guarantee")

        for label,pc in [("bad_thumb",APP+0x100),
                         ("bad_low",APP-1),
                         ("bad_high",APP+0x100000+1)]:
            candidate=bytearray(raw);vector(candidate,pc)
            f=d/(label+".bin");f.write_bytes(candidate)
            run("pack",f,d/(label+".flat.bin"),success=False)
        print("PASS invalid M33 Reset PC rejected at packing time")

        for length in (257*1024,MAX):
            candidate=bytearray(b"\xff"*length)
            vector(candidate)
            src=d/f"{length}.bin";out=d/f"{length}.flat.bin"
            src.write_bytes(candidate)
            run("pack",src,out)
            assert len(out.read_bytes())==length
            run("verify",out)
        print("PASS actual variable size 257KiB and full 488KiB limit")

        oversized=bytearray(b"\xff"*(MAX+4))
        vector(oversized)
        src=d/"over.bin";src.write_bytes(oversized)
        run("pack",src,d/"bad.bin",success=False)
        run("pack",d/"raw.bin",d/"raw.bin",success=False)
        print("PASS 488KiB MAX allocation and self-overwrite rejected")
if __name__=="__main__":main()
