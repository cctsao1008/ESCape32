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

def check_ram_flash_branches(objdump, boot, ram_start, ram_bytes):
    """Audit direct branch targets in SRAM-resident Flash routines.

    This deliberately does NOT claim proof against indirect jumps,
    literal/data fetches, NMIs, brown-out, or peripheral HW behavior.
    The only pre-erase Flash call permitted from the quarantined RAM
    wrapper is stage_prepare, which must precede the sector executor.
    """
    asm=run(objdump,"-d","--no-show-raw-insn",boot)
    end=ram_start+ram_bytes
    function=""
    direct=0
    preflight=0
    commit_calls=0
    for line in asm.splitlines():
        h=re.match(r"^\s*[0-9a-fA-F]+\s+<([^>]+)>:\s*$",line)
        if h:
            function=h.group(1)
            continue
        match=re.match(r"^\s*([0-9a-fA-F]+):\s+([A-Za-z][A-Za-z0-9_.]*)\s*(.*)$",line)
        if not match:continue
        address=int(match.group(1),16)
        if not (ram_start<=address<end):continue
        opcode=match.group(2).lower()
        operand=match.group(3)
        baseop=opcode.split(".")[0]
        if baseop not in ("bl","blx","b","bx","beq","bne",
                         "bcc","bcs","bhi","bls","bge","blt",
                         "bgt","ble","bmi","bpl","bvs","bvc"):
            continue
        if baseop=="bx" and operand.strip()=="lr":
            continue
        target=re.search(r"(?<![0-9a-zA-Z])(?:0x)?([0-9a-fA-F]{6,8})\s+<([^>]+)>",operand)
        if target is None:
            raise AssertionError("SRAM Flash code has unverifiable branch "+
                                 function+": "+line.strip())
        dest=int(target.group(1),16)
        callee=target.group(2).split("+")[0]
        if ram_start<=dest<end:
            direct+=1
            if function=="boot_am13e_update_commit_quarantined" and callee=="boot_am13e_commit_sectors":
                commit_calls+=1
            continue
        if (function=="boot_am13e_update_commit_quarantined" and
            callee=="boot_am13e_stage_prepare_for_commit" and
            commit_calls==0):
            # before IRQ disable / first erase; no erased Boot dependency
            preflight+=1
            continue
        raise AssertionError("SRAM Flash command reaches Flash/non-SRAM: "+
                             function+" -> "+callee+" @"+hex(dest))
    check(direct>=8 and preflight==1 and commit_calls==1,
          "Flash commit direct branches stay in SRAM_C after preflight")
    print("LIMIT: indirect/literal execution and on-silicon recovery remain unqualified")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--build",type=Path,required=True)
    p.add_argument("--nm",default="arm-none-eabi-nm")
    p.add_argument("--readelf",default="arm-none-eabi-readelf")
    p.add_argument("--objdump",default="arm-none-eabi-objdump")
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
    check("write" in b and "boot_am13e_flash_write" not in b and
          0<=b["write"]<0x4000,
          "Native Boot exports upstream Rel17 write(), not old private alias")
    check(all(symbol in b for symbol in
              ("main", "am13e_rel17_boot_main", "Reset_Handler")) and
          all(0<=b[symbol]<0x4000 for symbol in
              ("main", "am13e_rel17_boot_main", "Reset_Handler")) and
          all(symbol in f for symbol in
              ("main", "am13e_rel17_app_main", "Reset_Handler")) and
          all(APP<=f[symbol]<END for symbol in
              ("main", "am13e_rel17_app_main", "Reset_Handler")) and
          "am13e_rel17_app_main" not in b and
          "am13e_rel17_boot_main" not in f,
          "Each image links TI Reset_Handler, int main entry and its own Rel17 void dispatcher")
    ram=bs.get(".TI.ramfunc",(0,0))
    fw_ram=fs.get(".TI.ramfunc",(0,0))
    check(0xC18000<=ram[0]<ram[0]+ram[1]<=0xC20000 and
          0xC18000<=fw_ram[0]<fw_ram[0]+fw_ram[1]<=0xC20000 and
          ram[0]<=b.get("DL_Flash_program",0)<ram[0]+ram[1] and
          ram[0]<=b.get("DL_Flash_eraseSector",0)<ram[0]+ram[1],
          "Real Boot and APP Flash controller RAMFUNC physically in SRAM_C")
    check(all(ram[0]<=b.get(fn,0)<ram[0]+ram[1] for fn in
              ("boot_am13e_commit_sectors",
               "boot_am13e_update_commit_quarantined")) and
          "boot_am13e_update_commit_host_run" not in b,
          "Quarantined Boot sector executor and nonreturning entry reside in SRAM_C")
    check_ram_flash_branches(opt.objdump,boot,ram[0],ram[1])
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
