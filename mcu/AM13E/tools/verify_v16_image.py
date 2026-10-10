# SUPERSEDED v1.6 image-validation tool; historical, NOT in active CI.
# Use mcu/AM13E/tools/verify_rel17_image.py for Rel17 v1.4.
#!/usr/bin/env python3
"""AM13E reference v1.6 real FW1/Boot ELF + Packed BIN static integration gate.

Read actual ARM linker symbols and sections, not a synthetic metadata image.
This is a HOST/CI gate. It cannot qualify live motor, analog, Flash
power-loss timing or the electrical gate-driver/OC interface.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zlib

APP = 0x6000
SIG = 0x6400
HDR = 0x6500
CODE = 0x6800
FLASH_END = 0x80000
TRANSPORT = 0x7A000

def run(*args):
    return subprocess.check_output([str(s) for s in args], text=True)

def symbols(nm, elf):
    d = {}
    for line in run(nm, "--defined-only", elf).splitlines():
        m = re.match(r"^([0-9a-fA-F]+)\s+[A-Za-z]\s+(\S+)$",line)
        if m:
            d[m.group(2)] = int(m.group(1),16)
    return d

def sections(readelf, elf):
    d={}
    for line in run(readelf,"-SW",elf).splitlines():
        m=re.match(r"^\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+"
                   r"([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+"
                   r"([0-9a-fA-F]+)\s+",line)
        if m:
            d[m.group(1)]=(int(m.group(2),16),int(m.group(3),16))
    return d

def require(ok,message):
    if not ok:raise AssertionError(message)
    print("PASS",message)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--build",type=Path,required=True)
    p.add_argument("--nm",default="arm-none-eabi-nm")
    p.add_argument("--readelf",default="arm-none-eabi-readelf")
    a=p.parse_args()
    root=a.build
    boot=root/"boot/BOOT5_PB14.elf"
    fw=root/"AM13E_FW1_V16.elf"
    raw=(root/"AM13E_FW1_V16.bin").read_bytes()
    packed=(root/"AM13E_FW1_V16.am13e.bin").read_bytes()
    manifest=json.loads((root/"AM13E_FW1_V16.json").read_text())
    bs,fs=symbols(a.nm,boot),symbols(a.nm,fw)
    bsec,fsec=sections(a.readelf,boot),sections(a.readelf,fw)
    require(bs.get("__app_flash_start__")==APP and
            bs.get("__app_vector_start__")==APP,
            "Boot references APP_BASE and VTOR 0x6000")
    require(bs.get("__boot_flash_end__")==0x4000 and
            bs.get("__boot_storage_end__")==FLASH_END,
            "Boot preserved and flash limit 0x80000")
    require(fs.get("__app_flash_start__")==APP and
            fs.get("__app_vector_start__")==APP and
            fs.get("_cfg")==0x4000,
            "FW1 APP/VTOR 0x6000 and exclusive FW1 cfg 0x4000")
    require(fs.get("__app_flash_payload_end__",0)<=APP+TRANSPORT,
            "CMD_WINDOW transport can carry full 488KiB FW1 ELF payload")
    require(fsec.get(".intvecs",(None,0))[0]==APP and
            64<=fsec[".intvecs"][1]<=0x400,
            "TI M33 startup vector table fits before APP+0x400 marker")
    require(fsec.get(".signature")==(SIG,16) and
            fsec.get(".image_header")==(HDR,32) and
            fsec.get(".text",(0,0))[0]>=CODE,
            "ROM vector/marker/header/text sections match v1.6")
    require(bsec.get(".intvecs",(None,0))[0]==0 and
            bsec[".intvecs"][1]>=64,
            "Boot Reset vector table remains at Flash address 0")
    b_ram=bsec.get(".TI.ramfunc",(0,0))
    f_ram=fsec.get(".TI.ramfunc",(0,0))
    require(0xC18000<=b_ram[0]<b_ram[0]+b_ram[1]<=0xC20000 and
            0xC18000<=f_ram[0]<f_ram[0]+f_ram[1]<=0xC20000,
            "Boot and FW1 real Flash RAMFUNC sections execute from SRAM_C")
    require(b_ram[0]<=bs.get("DL_Flash_eraseSector",0)<b_ram[0]+b_ram[1] and
            b_ram[0]<=bs.get("DL_Flash_program",0)<b_ram[0]+b_ram[1],
            "Real TI Boot erase/program DriverLib routines linked into SRAM_C")
    require(0x6000<=fs.get("__ramfunct_load__",0)<0x80000 and
            fs.get("__ramfunct_load__") != fs.get("__ramfunct_start__"),
            "FW1 SRAM RAMFUNC has separate Flash load address")
    require(0x20000000<=fs.get("__data_start__",0)<0x20018000 and
            0x20000000<=fsec.get(".cfg",(0,0))[0]<0x20018000,
            "FW1 mutable data and cfg in SRAM_S, not persistent Flash")
    require(len(raw)>0x800 and len(packed)%16==0 and
            len(packed)<=TRANSPORT and len(packed)-len(raw)<16,
            "Packed image aligned, bounded and matches actual ARM raw length")
    sp,pc=struct.unpack_from("<II",packed,0)
    require(0x20000008<=sp<=0x20018000 and not sp%8 and
            pc&1 and CODE<=pc&~1<APP+len(packed),
            "Packed APP+0 MSP and Thumb Reset_Handler point to real firmware")
    require(packed[SIG-APP:SIG-APP+2]==b"\xea\x32" and
            packed[:SIG-APP]==raw[:SIG-APP] and
            packed[0x520:len(raw)]==raw[0x520:],
            "Pack operation changes ONLY metadata slots, not real vectors or code")
    header=packed[HDR-APP:HDR-APP+32]
    require(struct.unpack_from("<I",header,28)[0] == zlib.crc32(header[:28]) and
            struct.unpack_from("<I",header,16)[0] ==
                zlib.crc32(packed[:HDR-APP]+packed[HDR-APP+32:]),
            "Image/header CRCs match independent zlib calculation")
    require(manifest["vector_address"]==APP and
            manifest["signature_address"]==SIG and
            manifest["header_address"]==HDR and
            manifest["image_length"]==len(packed) and
            manifest["sha256"]==hashlib.sha256(packed).hexdigest(),
            "Manifest matches actual linked ELF-derived image")
    print("PASS AM13E reference v1.6 REAL Boot/FW1 software image static integration")

if __name__=="__main__":
    main()
