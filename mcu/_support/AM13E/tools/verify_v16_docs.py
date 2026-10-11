#!/usr/bin/env python3
"""Current Rev1.6 authority integrity and native repository architecture."""
from pathlib import Path
import csv
import hashlib
import re
import subprocess

ROOT = Path(__file__).resolve().parents[4]
BASE = ROOT / "mcu/_support/AM13E"
DIGESTS = {
    "Integration_Design.md": "6ebb98767056eb5b010f41f7644263a272fbfd0f8b33a045be97cfc1c884400c",
    "Integration_Mapping.md": "c68dbeb0ee0625da06f1ad7950ebabd81d9e6b70591ceb99f29622edcd3811e1",
    "Interface_Contracts.md": "70776e87fb712a528360974fc15fd1c5753516c550f5387c4a2a9dc833aff633",
}


def check_current_contract():
    authority = (BASE / "DESIGN_AUTHORITY.md").read_text()
    for name, digest in DIGESTS.items():
        if hashlib.sha256((BASE / "v1.6" / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError("Original Rev1.6 checksum mismatch: " + name)
        if name not in authority or digest not in authority:
            raise RuntimeError("Current authority provenance missing: " + name)
    mapping = (BASE / "v1.6/Integration_Mapping.md").read_text()
    ids = set(re.findall(r"^\| ((?:M|F|I|T|BTM|SYS)\d+) \|", mapping, re.M))
    rows = list(csv.DictReader((BASE / "REV16_REQUIREMENTS.csv").open()))
    if len(ids) != 90 or len(rows) != 90 or {r["id"] for r in rows} != ids:
        raise RuntimeError("Current Mapping must cover exactly 90 distinct IDs")
    for r in rows:
        if not all(r.values()) or r["hardware_status"] != "PENDING / NOT QUALIFIED":
            raise RuntimeError("Incomplete or unjustified qualification: " + r["id"])
    allowed={".github","boot","mcu","src","CMakeLists.txt","LICENSE","README.md"}
    # Audit repository architecture, excluding untracked CI build/log artifacts.
    tracked = subprocess.check_output(["git", "-C", str(ROOT), "ls-files", "-z"])
    actual={p.split("/", 1)[0] for p in tracked.decode().split("\0") if p}
    if actual!=allowed:raise RuntimeError("Repository root drift: "+str(actual^allowed))
    for directory,names in [("mcu/AM13E",{"config.c","config.h","config.cmake","config.ld","preset.c","hal"}),
                            ("boot/mcu/AM13E",{"config.c","config.h","config.cmake","config.ld","hal"})]:
        if {p.name for p in (ROOT/directory).iterdir()}!=names:
            raise RuntimeError("Native root drift: "+directory)
    print("PASS: exact Rev1.6 originals; 90 current IDs; no active historical crosswalk")
    print("SCOPE: document coverage is not firmware parity or hardware qualification")


if __name__ == "__main__":
    check_current_contract()
