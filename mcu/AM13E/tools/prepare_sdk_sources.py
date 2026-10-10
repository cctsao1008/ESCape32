#!/usr/bin/env python3
"""Apply narrow TI AM13E DriverLib/GCC startup correctness fixes at build time.

The vendor SDK is READ-ONLY. Emit modified copies under CMake's binary tree.
These are functional fixes, not -Wno-* warning suppression:
 - Initialize capture/compare mapping structures before their switch helpers;
   reject unsupported channel selectors before touching timer registers.
 - Convert the stack pointer through uintptr_t before storing its numeric
   32-bit value in the Cortex-M vector table's legacy TI pFunc slot.
The patch fails closed when TI changes the relevant source constructs.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

TIMER_REL = pathlib.Path("source/driverlib/am13e230x/dl_timer.c")
START_REL = pathlib.Path("source/device/am13e230x/src/startup_gcc_arm.c")


def exactly(source: str, old: str, new: str, count: int, filename: str) -> str:
    n = source.count(old)
    if n != count:
        raise ValueError(
            f"{filename}: expected {count} occurrences of {old!r}, found {n}; "
            "inspect the TI SDK revision before modifying its sources"
        )
    return source.replace(old, new)


def fixed_timer(source: str) -> str:
    name = "dl_timer.c"
    # Preserve all valid-channel behavior while preventing a partially
    # assigned local struct on invalid input.
    for name_of_func, param_type in (
        ("DL_Timer_initCaptureMode", "DL_Timer_CaptureConfig"),
        ("DL_Timer_initCaptureCombinedMode", "DL_Timer_CaptureCombinedConfig"),
        ("DL_Timer_initCompareMode", "DL_Timer_CompareConfig"),
    ):
        signature = (
            f"void {name_of_func}(GPTIMER_Regs *gptimer, "
            f"{param_type} *config)\n{{"
        )
        replacement = signature + (
            "\n    /* E62 integration: fail closed on unmapped channels. */"
            "\n    if (config->inputChan != DL_TIMER_INPUT_CHAN_0 &&"
            "\n        config->inputChan != DL_TIMER_INPUT_CHAN_1)"
            "\n        return;"
        )
        source = exactly(source, signature, replacement, 1, name)

    # Four declarations: two capture, one combined pair, one compare.
    # Zero initializes all members, not just the ones GCC points to.
    pattern = re.compile(
        r"(?m)^([ \t]*DL_Timer_Input_(?:Pair_)?Chan_Config[ \t]+"
        r"(?:captConfig|captPairConfig|inChanConfig));$"
    )
    matches = pattern.findall(source)
    if len(matches) != 4 or sorted(m.split()[-1] for m in matches) != [
        "captConfig", "captConfig", "captPairConfig", "inChanConfig"
    ]:
        raise ValueError(
            f"{name}: expected the four TI capture/compare locals; got "
            f"{matches!r}"
        )
    return pattern.sub(r"\1 = {0};", source)


def fixed_startup(source: str) -> str:
    name = "startup_gcc_arm.c"
    source = exactly(
        source, "    (pFunc)&__StackTop,",
        "    (pFunc)(uintptr_t)&__StackTop,",
        1, name
    )
    # ARM vector word 0 is an address, not a function; the pointer-sized
    # intermediate avoids ISO's direct object-pointer-to-function cast.
    return source


def write_if_changed(path: pathlib.Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk-root", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    sdk = args.sdk_root.resolve()
    out = args.output.resolve()
    for rel, transform in (
        (TIMER_REL, fixed_timer),
        (START_REL, fixed_startup),
    ):
        path = sdk / rel
        if not path.is_file():
            raise FileNotFoundError(f"SDK file missing: {path}")
        source = path.read_text()
        fixed = transform(source)
        write_if_changed(out / rel.name, fixed)
        print(f"AM13E SDK compatibility: {rel.name} corrected in build tree")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
