#!/usr/bin/env python3
"""Review real AM13E Boot/FW1 ELF and flat-image Rel17 v1.4 contract."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
APP=0x6000
END=0x80000
MAX=END-APP
def run(*args):
    return subprocess.check_output([str(v) for v in args],text=True)
def sym(tool,elf):
    d={}
    for line in run(tool,"--defined-only",elf).splitlines():
        m=re.match(r"^([0-9a-fA-F]+)\s+[A-Za-z]\s+(\S+)$",line)
        if m:d[m[2]]=int(m[1],16)
    return d
def sec(tool,elf):
    d={}
    for line in run(tool,"-SW",elf).splitlines():
        m=re.match(r"^\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+"
                   r"([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+"
                   r"([0-9a-fA-F]+)\s+",line)
        if m:d[m[1]]=(int(m[2],16),int(m[3],16))
    return d
def check(value,reason):
    if not value:raise AssertionError(reason)
    print("PASS",reason)
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--build",type=Path,required=True)
    p.add_argument("--nm",default="arm-none-eabi-nm")
    p.add_argument("--readelf",default="arm-none-eabi-readelf")
    opt=p.parse_args()
    root=opt.build
    boot=root/"boot/BOOT5_PB14.elf"
    app=root/"AM13E_FW1_REL17.elf"
    raw=(root/"AM13E_FW1_REL17.bin").read_bytes()
    flat=(root/"AM13E_FW1_REL17.flat.bin").read_bytes()
    meta=json.loads((root/"AM13E_FW1_REL17.json").read_text())
    b,f=sym(opt.nm,boot),sym(opt.nm,app)
    bs,fs=sec(opt.readelf,boot),sec(opt.readelf,app)
    check(b.get("__cfg_flash_start__")==0x4000 and
          b.get("__app_flash_start__")==APP and
          b.get("__app_vector_start__")==APP and
          b.get("__boot_flash_end__")==0x4000 and
          b.get("__boot_storage_end__")==END,
          "Boot CFG ID pointer=0x4000, APP vectors=0x6000, APP end=0x80000")
    check(f.get("_cfg")==0x4000 and f.get("__app_flash_start__")==APP and
          f.get("__app_vector_start__")==APP and
          f.get("__app_flash_payload_end__",END+1)<=END,
          "Real linked FW1 code/config within 488KiB APP allocation")
    check(fs.get(".intvecs",(None,0))[0]==APP and
          fs[".intvecs"][1]>=64 and
          fs.get(".text",(0,0))[0]>=APP+fs[".intvecs"][1] and
          fs.get(".text",(END,0))[0]<APP+0x800 and
          ".signature" not in fs and ".image_header" not in fs,
          "ELF contains normal contiguous Rel17 vector/text without v1.6 slots")
    check(bs.get(".intvecs",(None,0))[0]==0 and
          bs[".intvecs"][1]>=64,"Native Boot vector remains at 0x0000")
    ram=bs.get(".TI.ramfunc",(0,0))
    fw_ram=fs.get(".TI.ramfunc",(0,0))
    check(0xC18000<=ram[0]<ram[0]+ram[1]<=0xC20000 and
          0xC18000<=fw_ram[0]<fw_ram[0]+fw_ram[1]<=0xC20000 and
          ram[0]<=b.get("DL_Flash_program",0)<ram[0]+ram[1] and
          ram[0]<=b.get("DL_Flash_eraseSector",0)<ram[0]+ram[1],
          "Real Boot and APP Flash controller RAMFUNC physically in SRAM_C")
    check(8<=len(raw)<=MAX and len(flat)<=MAX and
          len(flat)%4==0 and 0<=len(flat)-len(raw)<=3 and
          flat[:len(raw)]==raw and
          flat[len(raw):]==b"\xff"*(len(flat)-len(raw)),
          "Flat image matches actual ELF objcopy bytes (<=3 transport pad)")
    sp,pc=struct.unpack_from("<II",flat)
    check(0x20000008<=sp<=0x20018000 and not(sp&7) and
          pc&1 and APP<=(pc&~1)<APP+len(flat),
          "Cortex-M33 vector references linked firmware")
    check(meta["cfg_marker_address"]==0x4000 and
          meta["image_length"]==len(flat) and
          meta["linked_raw_bytes"]==len(raw) and
          meta["app_allocation_max_bytes"]==MAX and
          meta["sha256"]==hashlib.sha256(flat).hexdigest() and
          "image_crc32" not in meta,
          "Sidecar manifest reports size only; no embedded header/CRC")
    print("PASS native Rel17 v1.4 ELF/Boot/flat image static audit")
if __name__=="__main__":
    try:main()
    except (AssertionError,OSError,KeyError,subprocess.CalledProcessError) as e:
        sys.exit("FAIL Rel17 flat image contract: "+str(e))
