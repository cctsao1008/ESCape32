#!/usr/bin/env python3
"""E1-C NON-FLASHABLE linker/startup/vector fixture for AM13E.

Builds a TEMPORARY, SYNTHETIC object and links it with the *actual*
TI startup and real irq_vectors.c, exercising linker sections.
It deliberately DOES NOT use src/*.c or supply ESC hardware backends.
It never writes a firmware image to the repository and deletes the
temporary ELF after its static checks. Fixture PASS != application PASS.
"""
import argparse
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

from toolchain_match import matched_tools

GCC_FLAGS = [
    "-march=armv8.1-m.main", "-mthumb",
    "-mfpu=fpv5-sp-d16", "-mfloat-abi=hard",
    "-Os", "-ffreestanding", "-ffunction-sections", "-fdata-sections",
]
CONFIG_FIXTURE = r"""
#include <stdint.h>
volatile uint32_t probe_initialized = 0x13240057u;
volatile uint32_t probe_zero;
__attribute__((used, section(".cfg")))
volatile unsigned char probe_configuration[72] = {0x32, 0xea};
__attribute__((noinline, section(".TI.ramfunc")))
void probe_ramfunc(void) { probe_zero += probe_initialized; }
void sys_tick_handler(void) { probe_zero += 1; }
void pend_sv_handler(void) { probe_zero += 2; }
void hard_fault_handler(void) { probe_zero += 3; for (;;) {} }
int main(void) {
    probe_ramfunc();
    return (int)(probe_zero + probe_configuration[0]);
}
"""


def run(cmd, *, cwd=None):
    r = subprocess.run(cmd, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, text=True, cwd=cwd)
    if r.returncode:
        raise RuntimeError("Command failed ({}):\n{}\n{}".format(
            r.returncode, " ".join(map(str, cmd)),
            (r.stdout + "\n" + r.stderr).strip()))
    return r.stdout


def symbol_table(nm, elf):
    result = {}
    for line in run([nm, "-g", "--defined-only", str(elf)]).splitlines():
        match = re.match(r"^([0-9a-fA-F]+)\s+([A-Za-z])\s+(\S+)$",
                         line.strip())
        if match:
            result[match.group(3)] = (int(match.group(1), 16),
                                      match.group(2))
    return result


def require(ok, msg):
    if not ok:
        raise RuntimeError(msg)
    print("[PASS]", msg)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--sdk-root", required=True, type=pathlib.Path,
                    help="local TI AM13E SDK root, NOT the assistant's zip")
    ap.add_argument("--repo", type=pathlib.Path,
                    default=pathlib.Path(__file__).resolve().parents[3])
    ap.add_argument("--build-dir", type=pathlib.Path, default=None,
                    help="CMake build directory; default repo/build-am13e")
    ap.add_argument("--gcc", default=None,
                    help="Optional compiler override, MUST match CMakeCache")
    ap.add_argument("--nm", default=None)
    ap.add_argument("--objcopy", default=None)
    args = ap.parse_args()
    repo = args.repo.resolve()
    build_dir = args.build_dir or repo / "build-am13e"
    gcc, nm, objcopy = matched_tools(build_dir, args.gcc, args.nm,
                                     args.objcopy)
    startup = (args.sdk_root / "source/device/am13e230x/src"
               / "startup_gcc_arm.c")
    adapter = repo / "mcu/AM13E/irq_vectors.c"
    linker = repo / "mcu/AM13E/linker_app_reference.ld"
    for file in (startup, adapter, linker):
        require(file.is_file(), "Found " + str(file))

    with tempfile.TemporaryDirectory(prefix="am13e-link-probe-") as td:
        tmp = pathlib.Path(td)
        fixture = tmp / "synthetic_linker_fixture.c"
        fixture.write_text(CONFIG_FIXTURE, encoding="utf-8")
        objs = []
        for file in (fixture, startup, adapter):
            obj = tmp / (file.stem + ".o")
            run([gcc, *GCC_FLAGS, "-DAM13E", "-c",
                 str(file), "-o", str(obj)])
            objs.append(obj)
        elf = tmp / "NON_FLASHABLE_LINKER_FIXTURE.elf"
        run([gcc, *GCC_FLAGS, "-nostdlib",
             "-Wl,--no-undefined", "-Wl,--gc-sections",
             "-Wl,-Map," + str(tmp / "fixture.map"),
             "-T" + str(linker), *map(str, objs), "-o", str(elf)])
        syms = symbol_table(nm, elf)
        def addr(name):
            require(name in syms, "ELF defines " + name)
            return syms[name][0]

        vector_addr = addr("__app_vector_start__")
        require(vector_addr == 0x6800, "Vector table at 0x6800")
        require(addr("_cfg") == 0x4000, "Persistent config Flash at 0x4000")
        cs, ce = addr("_cfg_start"), addr("_cfg_end")
        require(0x20000000 <= cs < ce <= 0x20018000,
                "Mutable .cfg stays in SRAM_S")
        require(0 < ce - cs <= 0x2000,
                "Config allocation fits reserved Flash_CFG length")
        ds, de = addr("__data_start__"), addr("__data_end__")
        dl = addr("__data_load__")
        require(0x20000000 <= ds < de <= 0x20018000,
                "Initialized .data has real SRAM VMA")
        require(0x6800 <= dl < 0x46000, ".data LMA stored in App Flash")
        rs, re_ = addr("__ramfunct_start__"), addr("__ramfunct_end__")
        rl = addr("__ramfunct_load__")
        require(0x00c18000 <= rs < re_ <= 0x00c20000,
                ".TI.ramfunc executes from SRAM_C")
        require(0x6800 <= rl < 0x46000,
                ".TI.ramfunc has App Flash load image")
        require(addr("_eod") <= 0x46000,
                "Image content stays within transport limit")
        for handler in ("HardFault_Handler", "PendSV_Handler", "SysTick_Handler"):
            require(syms.get(handler, (0, "?"))[1] in ("T", "t"),
                    handler + " is a strong text definition")
        require(addr("Reset_Handler") >= 0x6800,
                "TI startup reset handler is in Application Flash")

        vectors_bin = tmp / "vectors.bin"
        run([objcopy, "-O", "binary", "--only-section=.intvecs",
             str(elf), str(vectors_bin)])
        data = vectors_bin.read_bytes()
        require(len(data) >= 64, "M33 exception vector data present")
        words = struct.unpack_from("<16I", data)
        slots = {
            0: ("__StackTop", False),
            1: ("Reset_Handler", True),
            3: ("HardFault_Handler", True),
            14: ("PendSV_Handler", True),
            15: ("SysTick_Handler", True),
        }
        for index, (name, thumb) in slots.items():
            expected = addr(name) | (1 if thumb else 0)
            require(words[index] == expected,
                    "Vector[{}] points to {} (0x{:08x})".format(
                        index, name, expected))

        print("\nRESULT: SYNTHETIC LINKER / TI STARTUP / VECTOR PROBE PASS")
        print("LIMIT: actual Rel17 Application not linked; config persistence,")
        print("flash ECC, PRIMASK unmask, hardware IRQ and motors UNTESTED.")
        print("NOTE: temporary fixture ELF, MAP and vector BIN are deleted.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as e:
        print("[FAIL]", e, file=sys.stderr)
        sys.exit(1)
