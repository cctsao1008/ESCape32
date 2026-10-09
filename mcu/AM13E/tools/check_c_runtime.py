#!/usr/bin/env python3
"""Check which Rel17 C symbols the SELECTED ARM GCC libc actually exports.

Checks both libc_nano.a and libc.a; prints availability but does not
link firmware or claim that a symbol is safe/ABI compatible. Newlib
symbols depend on the installed GCC multilib and specs selection.
"""
import argparse
import pathlib
import re
import subprocess
import sys

AM13E_MULTILIB = ("-march=armv8.1-m.main", "-mthumb",
                  "-mfpu=fpv5-sp-d16", "-mfloat-abi=hard")

NEEDED = ("abs", "atoi", "itoa", "memcmp", "memcpy", "memmove", "memset",
          "stpcpy", "strcasecmp", "strlcpy", "strlen", "strsep", "strtol")


def run(args):
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       text=True)
    if r.returncode:
        raise RuntimeError("{}: {}".format(" ".join(args),
                                           r.stderr.strip()))
    return r.stdout


def definitions(nm, archive):
    out = run([nm, "--defined-only", "--extern-only", str(archive)])
    result = set()
    for line in out.splitlines():
        m = re.search(r"\b([A-Za-z])\s+([A-Za-z_][\w.$@]*)\s*$", line)
        if m and m.group(1).upper() not in ("U",):
            result.add(m.group(2))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--gcc", default="arm-none-eabi-gcc")
    p.add_argument("--nm", default="arm-none-eabi-nm")
    p.add_argument("--nano", action="store_true",
                   help="Prefer libc_nano.a for main summary")
    a = p.parse_args()

    found = {}
    for name in ("libc_nano.a", "libc.a", "libgcc.a"):
        raw = run([a.gcc, *AM13E_MULTILIB,
                   "-print-file-name=" + name]).strip()
        path = pathlib.Path(raw)
        if not path.is_file():
            print("[NOT FOUND]", name, "=>", raw)
            continue
        defs = definitions(a.nm, path)
        found[name] = defs
        print("[LIBRARY]", name, "=>", path,
              "({} exported definitions)".format(len(defs)))

    prefer = "libc_nano.a" if a.nano else "libc.a"
    if prefer not in found:
        print("[ERROR] Requested runtime archive unavailable:", prefer,
              file=sys.stderr)
        return 2
    chosen = found[prefer]
    missing = []
    for symbol in NEEDED:
        state = "DEFINED" if symbol in chosen else "MISSING"
        other = [name for name, defs in found.items()
                 if name != prefer and symbol in defs]
        suffix = (" (elsewhere: {})".format(",".join(other))) if other else ""
        print("[{}] {}{}".format(state, symbol, suffix))
        if state == "MISSING":
            missing.append(symbol)
    print("Selected archive:", prefer)
    print("Unresolved within that archive:", len(missing))
    print("LIMIT: archive symbol presence != ABI correctness, linked binary")
    print("        availability, licensing, or verified Rel17 behavior.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError) as exc:
        print("[ERROR]", exc, file=sys.stderr)
        sys.exit(2)
