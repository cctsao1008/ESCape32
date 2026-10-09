#!/usr/bin/env python3
"""Report globally undefined symbols after combining AM13E Application objects.

A U printed by nm on one object is not a whole-application missing symbol
when another object defines it. This script uses only actual ARM ELF symbols.
It does NOT test libc archive availability or assert that firmware can link.
"""
import argparse
import collections
import pathlib
import subprocess
import sys

LIBC_CANDIDATES = {
    "abs", "atoi", "itoa", "memcpy", "memcmp", "memmove", "memset",
    "stpcpy", "strcasecmp", "strlcpy", "strlen", "strsep", "strtol",
}
REL17_LINKER_SYMBOLS = {
    "_boot", "_cfg", "_cfg_start", "_cfg_end", "_eod", "_ram",
    "_rom", "_vec",
}
BOARD_SERVICES = {
    "adctrig", "compctl", "init", "initgpio", "initio",
    "initled", "ledctl", "hsictl", "hallcode", "io_serial",
    "io_analog",
}


def read_nm(nm_exe, path):
    result = subprocess.run(
        [nm_exe, "--extern-only", "--format=posix", str(path)],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            "{}: nm failed: {}".format(path, result.stderr.strip())
        )
    symbols = []
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 2 and len(fields[1]) == 1:
            symbols.append((fields[0], fields[1]))
    return symbols


def classify(name):
    if name.startswith("am13e_"):
        return "AM13E hardware/backend (no implementation yet)"
    if name in REL17_LINKER_SYMBOLS or name.startswith("__"):
        return "linker/startup-defined (requires production linker)"
    if name in BOARD_SERVICES:
        return "board/peripheral service (real implementation required)"
    if name in LIBC_CANDIDATES:
        return "C runtime/compatibility candidate (verify ARM toolchain)"
    return "unclassified (inspect before claiming link readiness)"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("objects", nargs="+", type=pathlib.Path)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    args = parser.parse_args()

    definition = collections.defaultdict(list)
    reference = collections.defaultdict(list)
    for obj in args.objects:
        if not obj.is_file():
            parser.error("ELF object not found: {}".format(obj))
        for name, kind in read_nm(args.nm, obj):
            if kind.upper() == "U":
                reference[name].append(str(obj))
            elif kind.upper() in {"T", "D", "B", "R", "W", "V", "A", "C", "I"}:
                definition[name].append((str(obj), kind))

    locally_resolved = sorted(set(reference).intersection(definition))
    missing = sorted(set(reference) - set(definition))
    duplicate = {
        name: locations
        for name, locations in definition.items()
        if sum(1 for _, kind in locations if kind.upper() not in {"W", "V"}) > 1
    }

    print("AM13E Application — combined ELF-object symbol inventory")
    print("Object count:", len(args.objects))
    print("Resolved across these objects:", len(locally_resolved))
    for name in locally_resolved:
        print("  [RESOLVED]", name)
    print("Still undefined after combining these objects:", len(missing))
    grouped = collections.defaultdict(list)
    for name in missing:
        grouped[classify(name)].append(name)
    for group, names in sorted(grouped.items()):
        print("\n" + group + " (" + str(len(names)) + "):")
        for name in names:
            print("  [OPEN]", name)

    for name, locations in sorted(duplicate.items()):
        print("\n[DUPLICATE STRONG DEFINITION] {}".format(name))
        for obj, kind in locations:
            print(" ", kind, obj)

    print("\nLIMIT: libc/syscalls, TI startup and linker are not included")
    print("unless their own object files are supplied as arguments.")
    print("Object resolution is NOT executable link or hardware PASS.")
    return 1 if duplicate else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as exc:
        print("[ERROR] {}".format(exc), file=sys.stderr)
        sys.exit(2)
