#!/usr/bin/env python3
"""Rev1.6 flat app packer: no embedded CRC/signature or fixed size."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import importlib.util

ROOT=Path(__file__).resolve().parents[5]
PACK=ROOT/"mcu/_support/AM13E/tools/pack_am13e_rel17.py"
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
    sys.dont_write_bytecode=True
    spec=importlib.util.spec_from_file_location("production_packer",PACK)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
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
        blocks=list(module.programming_blocks(raw))
        assert [b for b,_ in blocks]==list(range(4,10))
        assert b"".join(data for _,data in blocks)==packed
        config=bytes((i*17+3)&255 for i in range(4096))
        cfg=d/"explicit-config.bin";cfg.write_bytes(config)
        stream=d/"logical-stream.bin"
        run("stream",d/"raw.bin",stream,"--config",cfg,success=False)
        assert not stream.exists()
        run("stream",d/"raw.bin",stream,"--config",cfg,"--authorize-config")
        assert stream.read_bytes()==config+packed
        combined=list(module.programming_blocks(raw,config,authorize_config=True))
        assert [b for b,_ in combined]==list(range(10))
        assert b"".join(data for _,data in combined)==config+packed
        assert len(stream.read_bytes())==4096+len(packed)
        cfg.write_bytes(config[:-4])
        run("stream",d/"raw.bin",d/"bad-stream.bin","--config",cfg,
            "--authorize-config",success=False)
        run("stream",d/"raw.bin",cfg,"--config",cfg,
            "--authorize-config",success=False)
        assert not (d/"bad-stream.bin").exists()
        print("PASS Rev1.6 APP-only blocks start at4; explicit 4KiB Config+APP, actual size, no Reserved")
        if len(sys.argv)>1:
            tiny=bytearray(b"\xff"*12)
            vector(tiny,APP+9)
            fixture=d/"tiny-app.bin";fixture.write_bytes(tiny)
            subprocess.run([sys.argv[1],str(fixture)],check=True,
                           stdout=subprocess.PIPE,text=True)
            print("PASS tiny12-byte APP through production parser/writer; READ32 erased-tail padding")
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
        run("verify",d/"altered.bin") # Rev1.6 has NO image data CRC.
        print("PASS Rev1.6 intentionally has no payload CRC guarantee")

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
            blocks=list(module.programming_blocks(candidate))
            assert blocks[0][0]==4 and blocks[-1][0]==4+(length-1)//1024
            assert all(4<=block<=491 for block,_ in blocks)
            cfg_blocks=list(module.programming_blocks(candidate,config,authorize_config=True))
            assert cfg_blocks[0][0]==0 and cfg_blocks[-1][0]==blocks[-1][0]
        print("PASS actual variable size 257KiB and full 488KiB limit")

        oversized=bytearray(b"\xff"*(MAX+4))
        vector(oversized)
        src=d/"over.bin";src.write_bytes(oversized)
        run("pack",src,d/"bad.bin",success=False)
        run("pack",d/"raw.bin",d/"raw.bin",success=False)
        print("PASS 488KiB MAX allocation and self-overwrite rejected")
if __name__=="__main__":main()
