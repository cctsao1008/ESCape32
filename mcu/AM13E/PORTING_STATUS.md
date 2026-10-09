# AM13E ESCape32 Rel17 Application — porting ledger

This is an **application** work log, not a board-validation report.
Boot service remains independently versioned and is not changed here.

## E1-C PLL200: 9/9 ARM GCC PASS; TI DriverLib sources pending recheck (2026-10-09)

User-provided WSL logs confirm **9/9 AM13E Object Compile PASS**
(5 Rel17 + 4 platform adapters), without warnings or errors.
Cross-object references: **44 resolved, 62 unresolved**.
Open symbols: 37 AM13E backend, 6 board services,
13 C runtime, 4 linker, and 2 TI DriverLib symbols.

The clock function `am13e_app_clock_configure_xtal25` is now
resolved by the genuine 25MHz XTAL/SYSPLL200 Application Object.
The two new unresolved symbols were identified in the bundled
TI AM13E SDK:

- `DL_Common_delayCycles`: `dl_common.c`
- `DL_FRI_setReadWaitStates`: `dl_fri.c`, marked
  `RAMFUNC` and placed in `.TI.ramfunc`.

**Since the log**, genuine TI SDK `dl_common.c` and
`dl_fri.c` were added to the AM13E Object CMake target,
without dummy drivers or changing Boot/legacy selection.
`clock_xtal25_pll200.c` now waits for retained XTAL and
SYSPLL **GOOD-or-OFF** status before disabling either source.
These changes **have not yet been compiled in WSL**.

The next target is **11 Application Objects**, but neither
11/11 PASS nor a revised undefined-symbol count is asserted
until the user runs the build and symbol audit.

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail
cmake --build build-am13e --target AM13E -j"$(nproc)" \
  2>&1 | tee build-am13e/e1c-driverlib-build.log

mapfile -d '' am13e_objs < <(
  find build-am13e/CMakeFiles/AM13E.dir -type f -name '*.obj' -print0
)
printf 'AM13E object count: %d\n' "${#am13e_objs[@]}"

GCC=$(sed -n -E 's/^CMAKE_C_COMPILER:(FILEPATH|STRING)=//p' \
  build-am13e/CMakeCache.txt | head -n 1)
NM="${GCC%-gcc}-nm"

python3 mcu/AM13E/tools/check_object_symbols.py \
  --nm "$NM" "${am13e_objs[@]}" \
  | tee build-am13e/e1c-driverlib-symbols.log

"$NM" -g --defined-only "${am13e_objs[@]}" \
  | grep -E ' [TW] (DL_Common_delayCycles|DL_FRI_setReadWaitStates)$'
```

**Separate E1-C gates:** validate `.TI.ramfunc` RAM VMA/Flash
LMA and TI startup copy during actual Application linking.
The 25MHz XTAL, MCLK200, SYSPLL startup, X1/X2 IOMUX,
PRIMASK safety barrier, real MCU hardware and motor outputs
have NOT been exercised. Object PASS does not imply ELF
Link PASS or hardware clock qualification.

## E1-C 25MHz XTAL -> 200MHz MCLK implementation (2026-10-09)

**Decision confirmed:** LP-AM13E230 LaunchPad Y1 crystal
25MHz via X1/X2, SYSPLL CPU MCLK **200MHz** for Rel17.

The source implementation is now **committed but has NOT yet
been compiled with ARM GCC or validated on hardware**.

- `mcu/AM13E/clock_backend.h`: nominal 25MHz XTAL, 400MHz
  VCO, 200MHz MCLK interface constants and return contract.
- `mcu/AM13E/clock_xtal25_pll200.c`: actual TI SYSCTL/FRI
  register control. PDIV=/2, QDIV register=31 (effective ×32),
  SYSPLLCLK0 RDIV=/2, source=HFCLK; selects the factory
  feedback-input 8..16MHz tuning bin for fLOOPIN=12.5MHz.
- Increases Flash `FRDCNTL.RWAIT` to **3 before the MCLK
  transition**, via the real TI `DL_FRI_setReadWaitStates(3)`
  RAMFUNC DriverLib function; verifies readback.
- Configures MCLK2=/2 (100MHz) and MCLK4=/4 (50MHz ULPCLK)
  before 200MHz switch.
- Starts Y1/XTAL in crystal mode with nominal 9.984ms startup
  monitor, bounded polling for HFCLKGOOD and SYSPLLGOOD,
  then selects HSCLK and validates clock mux status.
  The finite polling limits are NOT calibrated millisecond
  timeouts. Crystal BOM/temperature/startup remain unverified.
- `mcu/AM13E/system_runtime.c` requires the 200MHz PLL
  status/nominal return before enabling the derived 16kHz
  SysTick (12,500 cycles). PRIMASK is **not** unmasked.
- Added clock backend to AM13E Object CMake target without
  enabling production ELF Linking or changing the five
  Rel17 source/Boot/Legacy configurations.

**TI DriverLib caveat:** `DL_FRI_setReadWaitStates` is a RAMFUNC
from `dl_fri.c`; the real Application Link must include and
initialize `.TI.ramfunc` prior to `init()`. Do not add an
empty replacement or link without startup section validation.
The current object inventory will also expose it as an external
symbol until the genuine DriverLib source is linked.

**Hardware P0:** 25MHz Y1 X1/X2 connection and load capacitance,
actual startup duration (nominal 156×64µs here), PLL frequency
measurement, voltage/temp corners, Flash execution timing,
Boot IRQ mask, pin ownership, and fault-safe motor outputs
are not verified. No user PCB or running AM13E Application
is available for physical testing.

### Next WSL ARM GCC gate

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail

cmake --build build-am13e --target AM13E -j"$(nproc)" \
  2>&1 | tee build-am13e/e1c-pll200-build.log

python3 mcu/AM13E/tools/check_object_symbols.py \
  build-am13e/CMakeFiles/AM13E.dir/src/*.obj \
  build-am13e/CMakeFiles/AM13E.dir/mcu/AM13E/*.obj \
  | tee build-am13e/e1c-pll200-symbols.log
```

**Expected source count: nine objects** (5 Rel17 + 4 TI
adapters: vector, reset-cause, system runtime, PLL200).
The exact undefined-symbol total must come from WSL;
`DL_FRI_setReadWaitStates` may remain open pending TI
DriverLib linkage. An Object Compile PASS does not
establish the 200MHz clock on silicon.

## E1-C clock correction — LaunchPad 25 MHz XTAL reference (2026-10-09)

**Current clock requirement overrides the earlier 8 MHz proposal below.**

Based on the TI **LP-AM13E230 LaunchPad User's Guide SLVUDH9**
§2.4 "Clock" (page 13), the selected reference is **25 MHz
crystal oscillator Y1**, across **X1 (PC16)/X2 (PC17)**.
The separate **J14 HFCLK_IN 4–48 MHz digital clock** input is NOT
selected. The historical 8 MHz XTAL contract and its
Datasheet/TRM range dispute are now **superseded**; both reviewed
document ranges include 25 MHz.

Changes on `am13e-port-v2`:

- `mcu/AM13E/clock_backend.h`: `AM13E_APP_XTAL_HZ =
  UINT32_C(25000000)` and
  `am13e_app_clock_configure_xtal25()` (declaration only).
- `mcu/AM13E/system_runtime.c`: calls the new 25 MHz
  XTAL clock backend; still derives **16 kHz SysTick** from the
  actual, backend-verified MCLK and fails closed on a source/status
  mismatch. No clock PLL or board IOMUX implementation was added.
- `mcu/AM13E/CLOCK_CONTRACT.md`: grounded in the LaunchPad
  Y1 + X1/X2 and HFCLK_IN distinction, with target CPU MCLK,
  SYSPLL and timing decisions still explicitly pending.
- Boot 32 MHz SYSOSC handoff check and safety PRIMASK gating are
  unchanged, as are the five Rel17 sources and all legacy targets.

**No WSL ARM rebuild has been received after the 25 MHz
contract change.** The last user `e1c-systick-symbols.log`
(8 objects, 43 resolved, 60 undefined) belongs to the
earlier 32 MHz SYSOSC integration *before* the external XTAL
contract changed. Do not present it as current 25 MHz compile
evidence. After a clean rebuild the new XTAL backend should
remain an intentional Link blocker until its real
implementation exists.

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail
cmake --build build-am13e --target AM13E -j"$(nproc)" \
  2>&1 | tee build-am13e/e1c-xtal25-compile.log
python3 mcu/AM13E/tools/check_object_symbols.py \
  build-am13e/CMakeFiles/AM13E.dir/src/*.obj \
  build-am13e/CMakeFiles/AM13E.dir/mcu/AM13E/*.obj \
  | tee build-am13e/e1c-xtal25-symbols.log
```

Acceptance at this stage: source/object integration only;
no target board clock, Application ELF, MCU pinmux, crystal
startup, safe motor output or ISR scheduling has been
verified on hardware.

## E1-C 8-object integration and updated 8 MHz XTAL requirement (2026-10-09)

User-supplied `e1c-systick-symbols.log` reports 8 objects,
**43 cross-object resolved** and **60 unresolved**:

| Category | Open symbols |
| --- | ---: |
| AM13E hardware/backend | 37 |
| Board/peripheral services | 6 |
| ARM C Runtime / compatibility | 13 |
| Production linker | 4 |

`init` and `am13e_app_motor_runtime_tick_init` are now
RESOLVED between objects. This is sufficient evidence for
**8-object cross-object symbol integration**; a fresh explicit
Build Log or successful build command is still required to
confirm the latest compiler run. The symbol log does not
verify on-target clock/timing behavior.

**Clock requirement changed:** external **8 MHz quartz XTAL** on
X1/X2 is the user-selected reference. The previous 32 MHz SYSOSC
is only the unchanged Boot handoff clock, *not* the final
Application clock. New `CLOCK_CONTRACT.md` documents a TI
documentation discrepancy: Datasheet Rev A explicitly allows
8 MHz (4–25 MHz feature range / 4–48 MHz electrical range),
while TRM Rev B describes 10–25 MHz for a crystal. Verify with
TI for the exact silicon before hardware qualification.
No external oscillator activation has been claimed.

**Code update (NOT WSL compiled yet):**
- New declaration-only `clock_backend.h` requires
  `am13e_app_clock_configure_xtal8()` from the actual
  X1/X2 / HFCLK / optional SYSPLL backend.
- `system_runtime.c` no longer hardcodes 32 MHz SysTick or
  2000 cycles. On entry `init()` validates Boot SYSOSC as a
  precondition, then requires XTAL-derived MCLK/HSCLK and
  `HFCLKGOOD`. SysTick reload is derived from the *verified*
  MCLK Hz, at 16 kHz; no blind assumption that XTAL=CPU MCLK.
- Real PLL/MCLK target, 8 MHz oscillator startup time,
  pinmux, power/bus divisors and clock proof still pending.
- The new unresolved symbol is an **intentional link blocker**.
  If no other symbols are emitted the next audit should show
  43 resolved and 61 undefined, but this is not yet verified.
- The independent `am13e_app_motor_runtime_enable_interrupts`
  board-safety barrier still must be implemented.

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail

cmake --build build-am13e --target AM13E -j"$(nproc)" \
  2>&1 | tee build-am13e/e1c-xtal-clock-contract-build.log

python3 mcu/AM13E/tools/check_object_symbols.py \
  build-am13e/CMakeFiles/AM13E.dir/src/*.obj \
  build-am13e/CMakeFiles/AM13E.dir/mcu/AM13E/*.obj \
  | tee build-am13e/e1c-xtal-clock-contract-symbols.log
```

**The updated clock source is a contract boundary, not an 8 MHz
oscillator running on silicon or an Application ELF.**

## E1-C reset-cause WSL PASS; baseline SYSOSC/SysTick backend awaiting compile (2026-10-09)

Newest user `e1c-resetcause-build.log` records
`[1/1] Building ... mcu/AM13E/reset_cause.c.obj` with no
diagnostics. The `e1c-resetcause-symbols.log` audit confirms:

| Evidence | Value |
| --- | ---: |
| ARM Object Compile | **7/7 PASS** |
| Cross-object resolved | **41** |
| Still undefined | **62** |
| AM13E backend declarations awaiting implementation | **38** |
| Board/peripheral services | **7** |
| ARM libc/compat candidates (archive checked separately) | **13** |
| Production linker symbols | **4** |

`am13e_app_motor_reset_flags` moved from OPEN to
cross-object RESOLVED, exactly as expected. This demonstrates
Source Integration; actual watchdog reset causes have not been
stimulated or measured on hardware.

The uploaded `compile_commands.json` contains **7** actual
AM13E translation units, each compiled by the SAME ARM GCC
**15.2.1** binary under `/home/build/ti/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin`;
AM13E compiles use `-march=armv8.1-m.main -mthumb
-mfpu=fpv5-sp-d16 -mfloat-abi=hard` and TI SDK include
paths. The extra `.ninja_*` and `build.ninja` files are
build metadata, not extra runtime pass evidence.

### Next independent board-neutral runtime slice

Added `mcu/AM13E/system_runtime.c` and selected it in
the AM13E Application Object target. This module:

- implements the previously missing `init(void)` via TI
  `DL_SYSCTL_getMCLKSource()` and `getClockStatus()`.
  It checks the inherited reset-default **32 MHz SYSOSC**;
  if conditions differ, execution halts closed instead of
  generating an invalid 16 kHz software timebase.
- implements `am13e_app_motor_runtime_tick_init()` using
  `DL_SYSTICK_init(2000)`, the actual 32 MHz / 16 kHz
  contract, CMSIS PendSV logical IRQ priority 8 and SysTick
  logical priority 0. It arms SysTick but **does not clear PRIMASK**.
- preserves the deliberately undefined
  `am13e_app_motor_runtime_enable_interrupts()` safety
  barrier; it must be supplied with real board output/IRQ
  qualification. This is NOT a motor-ready power-up routine,
  PLL clock plan, MCPWM timer setup, or an instruction to flash.

**This newly added Object has not yet been compiled in WSL.**
The latest verified count remains 7/7; after successful build
the next target will have 8 Objects and the predicted combined
symbol count is 43 resolved / 60 undefined, unless new
DriverLib dependencies are emitted. Both numbers must be
confirmed by the actual audit, not assumed.

Run in WSL:

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail

cmake --build build-am13e --target AM13E -j"$(nproc)" \
  2>&1 | tee build-am13e/e1c-systick-build.log

python3 mcu/AM13E/tools/check_object_symbols.py \
  build-am13e/CMakeFiles/AM13E.dir/src/*.obj \
  build-am13e/CMakeFiles/AM13E.dir/mcu/AM13E/*.obj \
  | tee build-am13e/e1c-systick-symbols.log
```

The Boot image, application linker reference, runtime IRQ
unmask barrier and Legacy MCU target paths remain separate.

## E1-C matching-toolchain verification CLOSED (2026-10-09)

Latest user WSL logs use the **same toolchain as the Application
CMake build**:

`/home/build/ti/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-gcc`

- `arm-none-eabi-gcc` version **15.2.1** (CMakeCache-matched).
- `arm-none-eabi-nm` and `arm-none-eabi-objcopy` from the same
  Toolchain directory.
- Temporary **synthetic** TI startup/Linker/Exception Vector Probe
  **PASS**: `0x6000` image, `0x6800` vector table, `0x4000`
  persistent config source, nonempty `.data` LMA/VMA, nonempty
  `.TI.ramfunc` Flash load/SRAM execution, SRAM `.cfg`, three
  strong Exception Handlers, and actual vector slots `0,1,3,14,15`.
- GCC **15.2.1** ARM v8-M main + FP hard ABI `libc.a` and
  `libc_nano.a`: **13/13 required exported symbols found**
  including `itoa`, `strlcpy`, `strsep`, `stpcpy`.
  This is archive introspection, NOT a final link.
- The earlier GCC10-vs-GCC15 uncertainty is **resolved** for
  the linker fixture and libc archive inspection.
- **Still not verified:** real Rel17 Application ELF/Flash image,
  hardware peripherals, PRIMASK behavior on silicon, Boot jump,
  config ECC/writeback and actual motor output.

### First genuine board-independent DriverLib slice (pending WSL build)

New `mcu/AM13E/reset_cause.c` reads actual
`DL_SYSCTL_getResetCause()` and translates
`DL_SYSCTL_RESET_CAUSE_BOOTWWDT0` into the Rel17 logical
watchdog/reset-arm flags. This is real TI SYSCTL access, **not**
a stub. The file was added to the AM13E Application CMake target;
five Rel17 Sources and Boot source selection are unchanged.

It does **not** configure the watchdog or define fault-reset policy
for non-WWDT causes. That policy needs safety review before motor
operation. Its ARM object is **not yet compiled on WSL**.

Before this commit the inventory contained **63 undefined symbols**
(39 AM13E backends, 7 board services, 13 libc, 4 linker).
After a successful object build, the reset-cause definition is expected
to resolve one backend symbol (nominally 62 remain). The exact
post-build number requires a fresh combined `nm` audit, especially
if DriverLib adds new dependencies.

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail
cmake --build build-am13e --target AM13E -j"$(nproc)" \
  2>&1 | tee build-am13e/e1c-resetcause-build.log
python3 mcu/AM13E/tools/check_object_symbols.py \
  build-am13e/CMakeFiles/AM13E.dir/src/*.obj \
  build-am13e/CMakeFiles/AM13E.dir/mcu/AM13E/*.obj \
  | tee build-am13e/e1c-resetcause-symbols.log
```

**Gate:** 7/7 objects PASS (five Rel17 plus Exception Adapter plus
Reset-cause DriverLib Adapter), **not** ELF/Hardware PASS.

## E1-C temporary linker probe PASS, runtime archive symbols PASS — compiler consistency recheck (2026-10-09)

User-provided `e1c-linker-probe.log` reports **SYNTHETIC LINKER /
TI STARTUP / VECTOR PROBE PASS** with actual link-time vector content:

- APP vectors at `0x00006800`, Flash cfg source at `0x00004000`;
  mutable `.cfg` inside SRAM_S.
- Nonempty initialized `.data` LMA in application Flash and VMA
  in SRAM_S; `.TI.ramfunc` LMA in application Flash, execution VMA
  in SRAM_C; payload within transport limit.
- Strong `HardFault_Handler`, `PendSV_Handler`, `SysTick_Handler`
  and correct actual vector slots `[0,1,3,14,15]`.
- Test deleted its temporary fixture, ELF, MAP and vector BIN.
  **No complete Rel17 Application ELF was linked.**

`e1c-libc-symbols.log` reports **13/13 required symbols** in
both checked `libc.a` and `libc_nano.a`, including `itoa`,
`strlcpy`, `strsep`, `stpcpy`. However, this specific log
identifies a GCC **10.3.1** multilib archive under
`/usr/lib/gcc/arm-none-eabi/10.3.1`, while the preceding
Application compilation command used TI-installed GCC **15.2**
under `$HOME/ti/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin`.
This is a **toolchain consistency issue**: symbol availability for the
Application's own GCC15/Newlib must be confirmed, and the existing
synthetic linker probe did not record its compiler version.

**Audit tooling corrected:** `toolchain_match.py` now derives
`arm-none-eabi-gcc`, sibling `nm` and `objcopy` from the
Application `build-am13e/CMakeCache.txt`. It prints the actual
compiler and version and rejects conflicting overrides.
`probe_app_linker.py` and `check_c_runtime.py` both use that
fail-closed toolchain resolver.

The previous fixture and libc findings remain valid for the toolchains
they actually exercised. They are **not yet upgraded to a matching
Application Toolchain PASS**. No backend was fabricated, and no
runnable firmware artifact was generated.

### Re-run with exactly the CMake toolchain

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail

python3 mcu/AM13E/tools/probe_app_linker.py \
  --sdk-root "$HOME/ti/am13e230x_sdk_26_01_00_03" \
  2>&1 | tee build-am13e/e1c-linker-matched-toolchain.log

python3 mcu/AM13E/tools/check_c_runtime.py \
  2>&1 | tee build-am13e/e1c-libc-matched-toolchain.log
```

**Gate:** both logs must begin with `[TOOLCHAIN] CMake compiler:`
pointing to the same real GCC used by `build-am13e`; confirm output
rather than assuming every Archive/fixture uses GCC15.
Even matching-toolchain probe PASS does not imply runtime hardware PASS.

## E1-C runtime barrier WSL PASS + linker/libc probes (2026-10-09)

Newest user WSL build: `[1/1] Building ... src/main.c.obj` PASS,
with no warnings. Combined object symbol inventory still resolves 40
internal references; open symbols now total **63**:

| Open class | Count |
| --- | ---: |
| AM13E required hardware/backend | 39 |
| Board/peripheral services | 7 |
| C runtime/compatibility candidates | 13 |
| Linker/startup | 4 |

The increase from 62 to 63 is **exactly**
`am13e_app_motor_runtime_enable_interrupts`, intentionally left
undefined until a safe board implementation exists.

Two opt-in, build-only audit tools were added:
- `mcu/AM13E/tools/probe_app_linker.py` temporarily compiles a
  **synthetic non-flashable fixture**, the real TI startup source
  and the committed strong `irq_vectors.c`. It links to
  `mcu/AM13E/linker_app_reference.ld`, checks actual GNU ARM
  linker placement of config RAM, `.data` LMA/VMA, RAMFUNC,
  vector slot values and image boundary; all fixture ELF/MAP/BIN
  files are deleted after the test. **Not Rel17 firmware**.
- `mcu/AM13E/tools/check_c_runtime.py` checks symbols exported
  by the installed Cortex-M33 hard-float Newlib archives.
  `itoa` / `strlcpy` availability is currently **unknown**;
  archive presence still does not prove actual link success.

Neither tool has been run on WSL. Do not claim that the linker
script or Newlib functions passed the actual GNU ARM checks.

### WSL next commands

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only

python3 mcu/AM13E/tools/probe_app_linker.py \
  --sdk-root "$HOME/ti/am13e230x_sdk_26_01_00_03" \
  | tee build-am13e/e1c-linker-probe.log

python3 mcu/AM13E/tools/check_c_runtime.py \
  | tee build-am13e/e1c-libc-symbols.log
```

Results must be assessed independently; a linker fixture PASS does
not remove any of the 46 open board/backend symbols or qualify
actual motor/power-stage hardware.

## E1-C WSL verified + nonproduction linker boundary (2026-10-09)

Latest user WSL output after `irq_vectors.c` integration:

- AM13E Application **6/6 OBJECT BUILD PASS**:
  five original Rel17 sources + one TI exception vector bridge.
- Combined symbol inventory: **40** references resolved between
  these objects, **62** still undefined: **38** declared AM13E
  backend hooks, **7** board/peripheral services, **13** C
  runtime/compatibility candidates, **4** linker symbols.
- `HardFault_Handler`, `PendSV_Handler`, `SysTick_Handler`
  each appear as strong `T` symbols in the relocatable adapter.
  Their `00000000` address in the relocatable object is normal
  for separate `-ffunction-sections`; **final Vector Table Slot
  values have not been inspected yet**.
- New production-*reference* `mcu/AM13E/linker_app_reference.ld`
  records current Boot image/metadata/vectors, mutable `.cfg`
  SRAM working image, TI startup `.data`/BSS/RAMFUNCT and
  256 KiB transport cap. It is **not wired to CMake**, not
  syntax-tested by ARM ld and **must not be flashed**.
- New explicit declaration-only
  `am13e_app_motor_runtime_enable_interrupts()` is called after
  Application motor/tick setup and requires the board backend to
  verify safe outputs, vectors and IRQ priorities before unmasking
  PRIMASK. No fake success/no-op implementation was added.
- **The last CMake+ARM build was before these latest linker/
  runtime-barrier edits.** Compile verification is needed again.

**Next WSL checks:**

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail
cmake --build build-am13e --target AM13E -j"$(nproc)" \
  2>&1 | tee build-am13e/e1c-runtime-barrier.log
python3 mcu/AM13E/tools/check_object_symbols.py \
  build-am13e/CMakeFiles/AM13E.dir/src/*.obj \
  build-am13e/CMakeFiles/AM13E.dir/mcu/AM13E/*.obj \
  | tee build-am13e/e1c-runtime-symbols.log
```

Do not interpret unresolved backend symbols as unexpected errors:
they are intentional link blockers until real board drivers exist.

## E1-C entry — symbols, TI vectors and handoff (2026-10-09)

User `nm -u` output confirms five Rel17 sources refer to four
distinct *classes* of externs: other Rel17 definitions; linker
metadata (`_cfg`, `_cfg_start`, `_cfg_end`, `_eod`);
toolchain C Runtime / compatibility (`itoa` and `strlcpy`
in particular need verification); and real board/backends
(`init`, `initgpio`, `initled`, `ledctl`, `compctl`,
`adctrig`, and `am13e_*`). This is not a linker error log
or evidence that the C Runtime satisfies every function.

TI `startup_gcc_arm.c` uses strong/weak CMSIS names
`SysTick_Handler`, `PendSV_Handler`, `HardFault_Handler`
and declares `extern int main(void)`. Rel17 has lowercase
`sys_tick_handler`, `pend_sv_handler`, `hard_fault_handler`.
Added **strong non-stub interrupt dispatchers** in
`mcu/AM13E/irq_vectors.c`, their prototypes in
`irq_vectors.h`, and the platform-specific adapter object
in the AM13E Application CMake target. For AM13E only, main
now has the TI startup-compatible `int main(void)` signature;
legacy targets retain the original signature.

The current Boot jumps to the app with PRIMASK interrupts
disabled. The SDK Startup does not re-enable them; the real
board-specific initialization needs to enable interrupts **after**
safe power-stage and vector/priority setup. This remains P0,
NOT fixed by the adapter alone.

Also created `mcu/AM13E/APP_LINK_CONTRACT.md` with the Boot-compatible
image and mutable config load requirements, and
`mcu/AM13E/tools/check_object_symbols.py` to distinguish
cross-object definitions from truly open symbols.

**No ARM rebuild or final ELF has been reported after this
E1-C integration work.** Previous E1-B five-object PASS is
preserved as the last actual compiler evidence. E1-C still
requires WSL compilation of the new vector bridge, final
startup/vector inspection, production linker, real drivers,
and hardware-safe handoff.

## Current gate status — E1-B complete (2026-10-09)

**Validated via user WSL ARM GCC build logs:** all FIVE Rel17
Application translation units compile to ARM Cortex-M33 objects.

| Source | Stage E1-B object build |
| --- | --- |
| `src/prog.c` | PASS |
| `src/telem.c` | PASS |
| `src/util.c` | PASS |
| `src/io.c` | PASS |
| `src/main.c` | PASS (3 nonfatal `-Wmissing-prototypes` warnings) |

The newest `e1b-main.log` shows only
`[1/1] Building C object CMakeFiles/AM13E.dir/src/main.c.obj`,
followed by warnings for `sys_tick_handler()`,
`pend_sv_handler()`, and `hard_fault_handler()`; no error
or failed build step. All earlier four object PASSes are from
preceding WSL build logs. **E1-B: 5/5 Object Compile PASS.**

This is **not** application link PASS, hardware driver PASS, legacy
regression PASS, or boot-to-application PASS. `add_target(AM13E AM13E)`
still creates an OBJECT library, not an executable. The motor,
input, telemetry, GPIO, Flash, and audio backends are explicitly
unimplemented. No production Application ELF/BIN exists yet.

### E1-C entry checklist (pending)

1. Perform *cross-object* unresolved symbol inventory using
   `arm-none-eabi-nm -u build-am13e/CMakeFiles/AM13E.dir/src/*.obj`
   and distinguish inter-object dependencies from real missing
   platform services. Do not silently weak-define missing drivers.
2. Match `sys_tick_handler` / `pend_sv_handler` /
   `hard_fault_handler` to the **actual TI startup vector** names.
   These three prototype warnings are not proof of IRQ-vector
   attachment. Review startup, IRQ precedence and fail-safe reset.
3. Define the production application memory and startup contract:
   current Boot uses reference Application flash start `0x6000`,
   metadata in the first sector, vectors at `0x6800`; **not**
   an authorization to reuse the dummy app-smoke linker.
   Audit ELF LMA/VMA, `.data` initialization, `.bss`, stack,
   VTOR, config Flash, ECC and application image packaging.
4. Implement **actual** AM13E hardware backends with safe outputs
   and board-qualified clocks/pinmux before linking a runnable image.
   An ELF obtained by dummy implementations is not accepted.
5. Run legacy STM32/AT32/GD32 regression checks independently.

Earlier 2/5, 3/5 and 4/5 sections below are historical gate
snapshots, **not** current status. Do not read historical “pending”
text as overruling this confirmed 5/5 result.

## E1-A: platform/header isolation

Evidence: first WSL ARM GCC build log after commits
`face920`, `2f6ca1`, and `bfd1d1` (2026-10-09).

- `src/common.h` selects the TI Cortex-M33 `soc.h` under `AM13E`;
  legacy libopencm3 headers remain unchanged under `#else`.
- `mcu/AM13E/config.h` is application-only (not Boot PB14 configuration).
- Root CMake selects `mcu/AM13E` for the AM13E application includes.
- `src/defs.h` legacy TIMx dead-time encoding / comparator mapping is
  excluded for AM13E. Missing `DEAD_TIME` / `COMP_MAP` is deliberate:
  no unverified value is introduced.
- There is **no** AM13E dummy TIM register model, board clock, dead time,
  power-stage pin mapping or peripheral implementation.
- No ARM rebuild has yet been reported after the `src/defs.h` adjustment.

## E1-B WSL input gate and motor-control source boundary (2026-10-09)

User WSL log `e1b-io.log`: `src/io.c.obj` PASS; `src/main.c.obj`
FAIL due to STM32 Timer, BEMF capture, clock, SysTick, WWDG and
RCC dependencies. With preceding builds, verified **4/5** objects
(`prog.c`, `telem.c`, `util.c`, `io.c`). No firmware link yet.

After that log, `src/main.c` was modified to isolate its remaining
STM32 peripheral accesses. The **latest main.c has not yet been
compiled in WSL**; do not call the Application 5/5 PASS until
the new ARM build log confirms it.

What is preserved as shared Rel17 control logic:

- 360-step sinusoidal startup sequencing and phase progression
- Six-step phase masks, comparator state, ZTC and direction
- Commutation interval smoothing, sync, ERPM and BEMF timing
- Throttle/Brake, slew-rate, duty ramp, lock and startup transitions
- ADC scaling, protection, PID, telemetry triggers and main loop
- 250ms uninterrupted-neutral arming behavior (hardware timer API)
- Boot/update implementation remains independent

New declaration-only `mcu/AM13E/motor_backend.h` defines:
- logical microsecond timebase for AM13E motor-control events
  (hardware timer prescaler and IRQ latency NOT determined);
- PWM / MCPWM duty, sinusoids, commutation, blanking and safe stop;
- BEMF/COMP capture callbacks into preserved Rel17 logic;
- SysTick 16kHz and reset-cause mapping;
- 250ms arming window and fault reset.

No board driver is implemented. Real PWM dead time, pin assignments,
output polarity, comparator input routing, DSHOT physical backend,
watchdog/reset cause, vector table, startup and Flash-safe config
persistence are all pending. No fabricated TI-register aliases were
added to eliminate compiler errors. The ARM compiler and legacy
regression build are still required after these edits.

**Static structural check only:** for `AM13E`, the updated
`src/main.c` conditional path contains no STM32 RCC/TIM/IFTIM/
STK/WWDG register symbols; AM13E motor API call names are declared
in `motor_backend.h`. This is NOT a compile result.

Next command:

```bash
cd ~/github/ESCape32
git switch am13e-port-v2
git pull --ff-only
set -o pipefail
cmake --build build-am13e --target AM13E -j"$(nproc)" -- -k 0 \
  2>&1 | tee build-am13e/e1b-main.log
```

Acceptance for E1-B: FIVE complete Rel17 source Translation Units
compile under ARM GCC. Link completion, performance parity and
motor functionality remain **separate gates**.

## E1-B WSL util gate and DSHOT/input isolation (2026-10-09)

WSL log `e1b-util.log`: `src/util.c.obj` PASS without warnings;
`src/io.c.obj` and `src/main.c.obj` FAIL on legacy STM32 registers.
Combined with the prior `prog.c.obj` and `telem.c.obj` reports,
**3/5** Rel17 objects have been verified. This status predates the
new input isolation below.

Latest `src/io.c` changes isolate STM32/AT32/GD32 capture timers,
DMA, watchdog, physical UART and CLI ISR. The shared Rel17 DSHOT
CRC, throttle and command-processing state machine remains compiled
for AM13E, and the legacy ISR calls the **same** command processor.
The shared function is always-inlined to avoid adding an explicit
function-call hop to the legacy time-sensitive DMA ISR.

`mcu/AM13E/io_backend.h` defines input callbacks for completed,
physically qualified 16-bit DSHOT packets, validated servo PWM pulse
widths, and complete CLI lines. A real backend MUST implement
`initio()` plus `am13e_app_io_watchdog_feed()`. Neither exists yet.
Unqualified capture frames must not be passed to the shared parser.
No dummy hardware behavior, clocks, UART pinmux or timers were added.

**Limitations**: AM13E does not yet have a physically implemented
bidirectional DSHOT turnaround/telemetry sender, timer capture/DMA
driver, receiver recovery, serial input decoder transport, aux brake
PWM, or watchdog/fault behavior. Legacy UART/SBUS/CRSF/EXBUS/HoTT
implementations remain in the guarded legacy branch. No AM13E board
functionality is asserted by Object Compile.

A static conditional-path scan shows no STM32 TIMER/DMA/USART/WWDG
references active in `src/io.c` for `AM13E`; this is **not** an ARM
GCC compile result. Stage E1-B remains **3/5 verified**, until WSL
confirms `src/io.c.obj` compiles.

## E1-B WSL telemetry gate and utility isolation (2026-10-09)

Second WSL log `e1b-telemetry.log` shows `telem.c.obj` **PASS**
(no diagnostic on its compilation), alongside prior `prog.c.obj` PASS.
`util.c`, `io.c`, `main.c` remain FAIL. Verified object total: **2/5**.

To address the `util.c` diagnostics, Stage E1-B isolates:

- Legacy GPIO/Hall/LED/HSI implementations and board mapping
  within `#if !defined(AM13E)`. Board GPIO entry points must
  be supplied by the actual AM13E backend, not empty weak functions.
- Flash storage `savecfg()`: its busy/ERTM gating remains common;
  AM13E delegates erase/program/verification to the **undefined**
  `am13e_app_cfg_commit()` backend. `resetcfg()` and
  `checkcfg()` are kept shared.
- `resetcom()`: requires an actual power-stage-safe implementation.
- Music and AU PCM: keep Rel17 score parsing, audio decode and
  blocking timing contract, but isolate TIM1/TIM6 programming
  via **declarations only** in `mcu/AM13E/util_backend.h`.
- CRC8/CRC16, Scale, Smooth and PID routines remain shared.
- A static preprocessing-path check found no legacy STM32
  RCC/Flash/TIM register accesses in the AM13E `util.c` branch;
  it did not run the ARM compiler.

**The updated `util.c.obj` has not yet been compiled on WSL.**
Do not claim 3/5 PASS until its ARM GCC output is received.
No config Flash page layout, dead time, clock or safe motor
output has been fabricated. Executable linking must remain
blocked until real peripherals are implemented.

## E1-B first slice: telemetry transport isolation (2026-10-09)

Changes are committed, **WSL ARM object build not yet verified**.

- `src/telem.c` retains the shared Rel17 iBUS, S.Port, MSB, HoTT,
  KISS and CRSF encoders/decoders. The full legacy USART/DMA branches
  are retained behind `#if !defined(AM13E)`.
- New `mcu/AM13E/telem_backend.h` is a **declarations-only** transport
  contract (UART mode initialization, asynchronous TX, byte TX, RX pause,
  RX-frame and TX-complete callbacks). There is no driver implementation.
- The AM13E parser callbacks keep access to the existing `iobuf`.
  The backend must preserve half-duplex direction/turnaround,
  receive-mode rearming, asynchronous buffer lifetime and delayed HoTT
  transmission. These are not hardware-qualified.
- Static conditional-path scan: **no USART1/DMA1/STM32 register references
  active in `telem.c` with `AM13E` defined**; all six protocol paths
  remain included. This is a structural check, NOT ARM GCC PASS.
- Since `am13e_telem_hw_*` functions lack implementation intentionally,
  a future production executable must not link successfully until a real
  backend is supplied.

**Immediate gate:** rebuild AM13E objects in WSL; `telem.c.obj`
is expected to compile, but this has not been proven yet.

## First actual object-compile result (before defs.h adjustment)

| Rel17 source | ARM GCC object status | First real blocking dependency |
| --- | --- | --- |
| `src/prog.c` | PASS | No compile-blocking peripheral reference |
| `src/telem.c` | FAIL | STM32-style USART1, DMA and serial timeout |
| `src/util.c` | FAIL | RCC, Flash configuration writes, TIM1/TIM6 audio and reset |
| `src/io.c` | FAIL | Timer input capture, DMA, DShot, USART, watchdog |
| `src/main.c` | FAIL | TIM1 complementary PWM, BEMF timer, SysTick/ISR, watchdog |

The prior `prog.c` PASS is **object compilation only**; configuration
storage / all external references have not been linked or exercised.

The absent STM32-style register names are evidence that AM13E's independent
hardware boundary is working, **not** reason to add synthetic aliases.
The branch is not yet E1-B PASS.

## Dependency contracts awaiting real backends

| Service | Rel17 entry points / examples | Required AM13E backend | Hardware decisions pending |
| --- | --- | --- | --- |
| Input capture and DShot | `initio`, `iotim_isr`, `dshotirq` | Input timer/capture/DMA/interrupts | Pins, capture resolution, DShot modes |
| Telemetry transport | `inittelem`, `sendtelemdata` | UART/DMA/IRQ transport | Physical UART, pinmux, baud/inversion |
| Motor commutation | `nextstep`, `iftim_isr` | MCPWM + COMP/TIMG | Gate driver, phase and BEMF mapping |
| Emergency shutdown | `hard_fault_handler`, `resetcom` | Force outputs off + reset/watchdog | Polarity, fault line, fail-safe contract |
| Config persistence | `savecfg`, `resetcfg` | Flash parameter driver | Config partition, erasure granularity, ECC |
| Runtime timing | `sys_tick_handler`, `delay` | SysTick / IRQ priority | Clock source, scheduler tick |

## Next integration order

1. Preserve `src/prog.c` object PASS and remove the AM13E-only
   `-Wundef` warnings by isolating legacy `TIM_DTG` and `COMP_MAP`.
2. Isolate `src/telem.c` protocol encoding from its STM32 UART/DMA
   transport. AM13E transport APIs may be **declared** but must not be
   given no-op implementations to create false runtime success.
3. Repeat for `src/io.c` and `src/util.c`, with `src/main.c`
   handled as a separate commutation/PWM boundary.
4. E1-B PASS requires five meaningful ARM object files and dependency
   inventory, not simply compilation of five empty files.
5. ELF linking, board-safe outputs, firmware update, and hardware testing
   are separate later gates.

## Local verification

```bash
cmake -S . -B build-am13e -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
set -o pipefail
cmake --build build-am13e --target AM13E -j"$(nproc)" -- -k 0 \
    2>&1 | tee build-am13e/e1b-diagnostic.log
```

Expected at the current gate: `src/prog.c` can compile; other sources
still need hardware boundary isolation. Do not assert build success
without actual WSL results.
