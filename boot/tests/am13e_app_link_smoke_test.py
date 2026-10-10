#!/usr/bin/env python3
"""Real ARM ELF -> flat Rel17 v1.4 APP BIN: NO image header or CRC."""
from pathlib import Path
import json
import re
import struct
import subprocess
import sys

APP=0x6000
MAX=0x7A000
def check(v,why):
    if not v:raise AssertionError(why)
    print("PASS",why)
def sections(tool,elf):
    txt=subprocess.check_output([str(tool),"-h",str(elf)],text=True)
    result={}
    for line in txt.splitlines():
        m=re.match(r"^\s*\d+\s+(\.[\w.]+)\s+([\da-fA-F]+)\s+([\da-fA-F]+)\s+",line)
        if m:result[m[1]]=(int(m[3],16),int(m[2],16))
    return result
def main():
    if len(sys.argv)!=6:
        raise SystemExit("usage: smoke-test RAW FLAT MANIFEST OBJDUMP ELF")
    raw,flat,manifest,objdump,elf=map(Path,sys.argv[1:])
    b=raw.read_bytes();p=flat.read_bytes()
    m=json.loads(manifest.read_text())
    sec=sections(objdump,elf)
    check(sec[".intvecs"][0]==APP and sec[".intvecs"][1]>=8,
          "Rel17 APP M33 vectors start at 0x6000")
    check(".signature" not in sec and ".image_header" not in sec,
          "No v1.6 APP signature or CRC-header sections")
    check(sec[".text"][0]>=APP+sec[".intvecs"][1] and
          sec[".text"][0]<APP+0x800 and sec[".text"][1]>0,
          "Text immediately follows linked vectors, not fixed 0x6800")
    check(len(b)>=8 and len(p)<=MAX and len(p)-len(b) in range(4) and
          len(p)%4==0 and p[:len(b)]==b and
          p[len(b):]==b"\xff"*(len(p)-len(b)),
          "Flat image length derives from actual linked binary (<=488KiB)")
    sp,pc=struct.unpack_from("<II",p)
    check(sp==0x20018000 and pc&1 and APP<=pc&~1<APP+len(p),
          "Native linked Cortex-M33 MSP and Thumb Reset PC")
    check(m["image_length"]==len(p) and
          m["linked_raw_bytes"]==len(b) and
          m["app_allocation_max_bytes"]==MAX and
          m["cfg_marker_address"]==0x4000,
          "External manifest records actual length, capacity and Cfg address")
    print("PASS Rel17 v1.4 flat vector-first ARM ELF smoke")
if __name__=="__main__":
    try:main()
    except (AssertionError,OSError,KeyError,subprocess.CalledProcessError) as exc:
        sys.exit("FAIL Rel17 flat ARM smoke: "+str(exc))
