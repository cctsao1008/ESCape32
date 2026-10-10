#!/usr/bin/env python3
"""Guard functional source filenames within the AM13E port."""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
FORBIDDEN = re.compile(
    r"(?:^|_)(?:gpio|adc|pb\d+|pa\d+|xtal\d+|pad)(?:_|$)",
    re.IGNORECASE,
)

def main() -> int:
    files = sorted(path for path in ROOT.rglob("*")
                   if path.is_file() and path.suffix in (".c", ".h"))
    bad = [path.relative_to(ROOT) for path in files
           if FORBIDDEN.search(path.stem)]
    if bad:
        for path in bad:
            print("FAIL: AM13E source filename embeds MCU/board detail:", path)
        return 1
    print(f"PASS: {len(files)} AM13E C/H filenames are function-oriented")
    return 0

if __name__ == "__main__":
    sys.exit(main())
