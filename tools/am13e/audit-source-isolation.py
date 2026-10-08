#!/usr/bin/env python3
"""Audit all ESCape32 src/*.c files, including paths absent from E62 CMake.
Read-only: do not automatically wrap portable modules or bypass safety gates.
"""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
portable = {
    "esc_math.c", "esc_config.c", "esc_cmd_parse.c",
    "esc_param_access.c", "esc_param_metadata.c",
}
mixed = {"main.c", "prog.c"}
legacy = {"io.c", "telem.c", "util.c"}
issues = []
for path in sorted((root / "src").glob("*.c")):
    name = path.name
    data = path.read_text(encoding="utf-8")
    if name in portable:
        kind = "portable"
    elif name in mixed:
        kind = "mixed/platform-conditional"
        if "ESCAPE32_AM13E" not in data:
            issues.append(f"{path.relative_to(root)}: missing AM13E conditional")
    elif name in legacy:
        kind = "legacy/platform-isolated"
        if not re.search(r"^#if defined\(ESCAPE32_AM13E\)", data, re.M):
            issues.append(f"{path.relative_to(root)}: missing file-scope AM13E branch")
    else:
        kind = "UNREVIEWED"
        issues.append(f"{path.relative_to(root)}: classification required")
    print(f"[{kind}] {path.relative_to(root)}")
if issues:
    print("SOURCE ISOLATION AUDIT INCOMPLETE", file=sys.stderr)
    for item in issues:
        print(" - " + item, file=sys.stderr)
    sys.exit(1)
print("SOURCE ISOLATION INVENTORY PASS (does not prove peripheral port complete)")
