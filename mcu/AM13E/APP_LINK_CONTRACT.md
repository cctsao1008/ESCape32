# AM13E ESCape32 Rel17 Application — Link and Startup Contract (E1-C)

Status: **design / integration requirements only**. No production
Application ELF/BIN, hardware validation or vector-table inspection
has been reported.

## E1-C reference linker source (NOT ENABLED)

`mcu/AM13E/linker_app_reference.ld` is a separate, application-specific
**non-production linker reference**, based on the existing Boot image
contract and TI Startup's actual data/BSS/ramfunc symbols. It is
**not wired** to `add_target(AM13E AM13E)` yet, because real GPIO,
MCPWM, COMP/BEMF, input, telemetry, Flash persistence and fault-handling
backends remain undefined. Do not create fake drivers merely to force
this image to link.

The reference linker places the mutable `.cfg` object in SRAM with
`NOLOAD` and exports `_cfg` as the separate persistent Flash source;
Rel17's own `main()` copies the config into SRAM, retaining the
existing control flow. This still needs proof of default recovery,
length/alignment, ECC and safe persistence on actual Flash hardware.

**A temporary synthetic fixture passed GNU ARM linking,
startup-section placement and actual exception-vector-slot checks
using the SAME GCC 15.2.1 toolchain as the Application's CMake build.**
The synthetic linker test validated genuine `.data` / `.TI.ramfunc`
load images and strong CMSIS exception handlers.
All 13 Rel17 C Runtime candidates are exported by the same GCC 15.2.1
toolchain's `libc.a` and `libc_nano.a` archives, but archive
availability is not application executable linking.
The full Rel17 source objects, real board backend implementations,
Flash/ECC, real-time behavior and hardware have NOT been linked
or validated. This remains a **non-production reference**.

## Toolchain provenance gate (2026-10-09) — PASS

The re-run `e1c-linker-matched-toolchain.log` explicitly identifies
the CMake-selected **ARM GCC 15.2.1**, with matching `nm` and
`objcopy`. The synthetic Linker / TI Startup / Vector Probe
completed PASS with vectors at `0x6800` and correct actual
Reset/HardFault/PendSV/SysTick slot entries.

The re-run `e1c-libc-matched-toolchain.log` uses the same GCC15.2.1
hard-float multilib and finds **13/13** required Rel17 C Runtime
symbol definitions in both `libc.a` and `libc_nano.a`.

Remaining P0 issues: actual Application ELF, hardware-safe interrupt
activation, real gate-driver outputs/clock and BEMF/ADC routing,
persisted `.cfg` semantics and Boot-to-Application execution.
The linker layout remains non-production until these are addressed.

## 1. Existing image contract to preserve

The current Boot reference defines:

| Flash region | Address | Purpose |
| --- | --- | --- |
| Boot | `0x00000000..0x00003FFF` | Existing Boot image (16 KiB) |
| Reserved parameter sectors | `0x00004000..0x00005FFF` | Reference 8 KiB config storage |
| Application image start | `0x00006000` | Deferred-commit `0x32EA` signature |
| Image metadata | `0x00006100` | 32-byte packed-image header |
| Application vectors | `0x00006800` | M33 initial MSP and Reset PC |
| Boot main Flash extent | through `0x0007FFFF` | Device reference, not write allowance |

The current packed transport is limited to **256 KiB from `0x6000`**,
even if physical MCU Flash extends farther. Do not infer all 512 KiB
is available to the running Application.

This address map is an **existing Boot interface**, subject to board
qualification; it is not a reason to reuse
`boot/mcu/AM13E/linker_app_smoke.ld` as production linker.
That smoke linker intentionally asserts `SIZEOF(.data)==0` and is
NOT a Rel17 Application linker.

## 2. Required production Linker symbols and sections

Rel17's `mcu/common.ld` historically maps `.cfg >ram AT>cfg` and exports
`_boot`, `_cfg`, `_rom`, `_ram`, and `_eod`. In contrast to
a const-only metadata header, `Cfg cfg` is **mutable Application
configuration**. Its lifetime and default/persistent copy path
must be preserved. The AM13E linker must not accidentally place the
mutable `cfg` object in read-only Flash.

At minimum, a real app linker must define/handle:

- `_cfg`, `_cfg_start`, `_cfg_end`: Rel17's configuration flash
  source and writable RAM image; prove flash sector allocation, ECC,
  load/erase granularity, CRC/readback, and alignment.
- `_eod`: end of Application binary/audio payload as used by Rel17.
- `.intvecs` at `0x6800` with valid initial SRAM stack and
  Thumb Reset_Handler.
- `.text`, `.rodata`, `.ARM.exidx` at valid Application Flash.
- `.data` in writable SRAM with an actual Flash LMA load image.
- `.bss` in SRAM and correctly cleared.
- `.TI.ramfunc` if used, with copy symbols matching TI startup;
  define the copy symbols even when the section is empty.
- `__StackTop` and all TI Startup data/bss/ramfunct symbols,
  keeping code/stack/data within device memory regions.
- An appropriate `-nostartfiles` / startup/CRT/library contract;
  test with actual final ELF/MAP, not object compilation.

**Danger:** The original `mcu/common.ld` uses a standalone
`cfg` *load region*. The application image packer and Boot Flash
partition must not inadvertently program/erase this config storage
during routine firmware updates or reset.

## 3. TI SDK Startup exception names

TI AM13E GCC startup uses weak
`SysTick_Handler`, `PendSV_Handler`, and `HardFault_Handler`.
It does **not** use the legacy libopencm3 names
`sys_tick_handler`, `pend_sv_handler`, `hard_fault_handler`.

A strong, non-stub adapter was added in:

- `mcu/AM13E/irq_vectors.c`
- `mcu/AM13E/irq_vectors.h`

The adapter directly invokes the real Rel17 handlers.
The AM13E application target compiles this sixth, platform-specific
object alongside the five Rel17 sources. It does NOT touch the
existing Boot or Legacy build paths.

TI startup declares `extern int main(void)`; the AM13E path now uses
`int main(void)`, while legacy targets retain `void main(void)`.

**Final ELF checks** must confirm:
1. Startup `.intvecs` is retained at `0x6800`.
2. All three adapter handlers are **strong** symbols, not weak.
3. The actual three vector slot words resolve to those handler symbols.
4. Rel17 callbacks are not removed by `--gc-sections`.
5. Hardware-specific IRQ vectors for MCPWM/ECAP/ADC/UART are attached
   only after the genuine backend exists; do not override with no-ops.

## 4. Boot-to-Application interrupt state — blocker

`boot/mcu/AM13E/app.c` disables interrupts with `__disable_irq()`
before setting VTOR, MSP and branching to the Application
Reset_Handler. The bundled TI `startup_gcc_arm.c` copies
`.data` / `.TI.ramfunc`, clears `.bss`, and calls `main()`;
it does **not** re-enable PRIMASK.

Therefore the Application **must** explicitly re-enable IRQs
only after board startup has installed a safe motor-output state,
configured vectors/priorities and relevant peripheral drivers.
Otherwise a real 16 kHz SysTick and PendSV scheduling cannot run.
Do not enable interrupts speculatively before safety initialization.
The AM13E Application now calls the **declaration-only**
`am13e_app_motor_runtime_enable_interrupts()` immediately after
`am13e_app_motor_runtime_tick_init()`, after the Application's
input/telemetry and power-stage init calls. That required board backend
must validate safe output gating, vector setup, priorities and pending
faults *before* clearing PRIMASK. There is deliberately no fallback
implementation or unconditional `__enable_irq()` in generic code.

The integration point is now explicit, but **real unmasking remains
unimplemented and untested**; failure to implement it must prevent
Link PASS. This requirement is P0 for the boot handoff smoke test.

TI's SystemInit call is also commented out in the inspected SDK
startup. Board clock and power initialization remains an explicit
Application platform obligation, not a guaranteed TI startup service.

## 5. Symbol categories after E1-B (before including new adapter object)

- Inter-Rel17 references: `cfg`, `checkcfg`, `scale`,
  `execcmd`, `savecfg`, `sendtelem`, etc. Resolved when all
  five objects are considered together.
- Linker: `_cfg`, `_cfg_start`, `_cfg_end`, `_eod`.
- C Runtime/compat candidates: `abs`, `atoi`, `itoa`,
  `memcpy`, `memcmp`, `memmove`, `memset`, `stpcpy`,
  `strcasecmp`, `strlcpy`, `strlen`, `strsep`, `strtol`.
  Especially `itoa` and `strlcpy` are **not guaranteed** to be
  in a given ARM C Runtime. Inspect actual libc/libgcc archives and
  perform a real final link before claiming their availability.
- Board services: `init`, `initgpio`, `initled`, `ledctl`,
  `compctl`, `adctrig`, and `initio`.
- Unimplemented driver interfaces: all referenced
  `am13e_app_motor_*`, `am13e_app_audio_*`,
  `am13e_telem_hw_*`, `am13e_app_cfg_commit`,
  `am13e_app_commutation_reset`, and
  `am13e_app_io_watchdog_feed`.

The tools `mcu/AM13E/tools/check_object_symbols.py` provides
a reproducible, **symbol-only** cross-object inventory.

## 6. Next WSL check

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail
cmake --build build-am13e --target AM13E -j"$(nproc)" \
    2>&1 | tee build-am13e/e1c-vector-compile.log

python3 mcu/AM13E/tools/check_object_symbols.py \
    build-am13e/CMakeFiles/AM13E.dir/src/*.obj \
    build-am13e/CMakeFiles/AM13E.dir/mcu/AM13E/*.obj \
    | tee build-am13e/e1c-combined-symbols.log

arm-none-eabi-nm -g --defined-only \
    build-am13e/CMakeFiles/AM13E.dir/mcu/AM13E/irq_vectors.c.obj \
    | grep -E ' (T|W) (SysTick_Handler|PendSV_Handler|HardFault_Handler)$'
```

**Pass criteria for this step**: all five Rel17 Objects remain
compilable; the real vector bridge compiles with exactly three strong
CMSIS exception names and calls the actual Rel17 handlers.

Not pass criteria: final link, no unresolved backends, actual vector
table, runtime interrupts, memory writes or motor output.
