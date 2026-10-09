# AM13E ESCape32 Rel17 — Application / Boot / Link Contract

**Status: architecture-alignment review; production linker NOT ready.**
The earlier synthetic linker/real TI RAMFUNC tests remain PASS for
a **historical Boot-format reference only**, not for the complete
SW Architecture Baseline v1.6.

## 1. Source authority and build boundary

1. **Software Architecture Baseline v1.6** owns normal ESC
   behavior, FW1/FW2 separation, boot/update policy and Flash regions.
2. **Hardware Architecture Baseline v1.6** owns AM13E peripheral
   instances, physical pin map, HFXT, sensing and protection routes.
3. **ESCape32 Rel17** owns the FW1 Application and is the Build/
   source-level functional baseline.
4. **TI SDK / CMSIS / DriverLib** provides the AM13E low-level MCU
   support layer (same role as libopencm3 on the legacy targets).
   A TI SDK example Application, generated SysConfig or standalone
   build system is not the source of product architecture.
5. The existing AM13E **Boot port** may be used to check Flash API,
   startup/VTOR, handoff and firmware image mechanics, but does
   not supersede the two v1.6 architecture baselines.

Canonical application entry: root ESCape32 CMake
`add_target(AM13E AM13E)`; the current target is an OBJECT library
only. No runnable Rel17 ELF/BIN has been produced.

## 2. SW Baseline v1.6 Flash policy — authoritative

| Region | Start | End (inclusive) | Capacity |
| --- | --- | --- | ---: |
| Common boot | `0x00000000` | `0x00003FFF` | 16 KiB |
| FW1 parameters | `0x00004000` | `0x00004FFF` | 4 KiB |
| FW2 parameters | `0x00005000` | `0x00005FFF` | 4 KiB |
| Single application (FW1 **OR** FW2) | `0x00006000` | `0x0007FFFF` | 488 KiB |

The FW1/FW2 parameter sectors are independently owned and
**preserved across application reflashing**. No A/B image,
bank swap, rollback slot or runtime firmware selector is selected.

The Baseline requires a fixed application-base vector/startup
contract; the allocated 488 KiB Flash region does **not** by
itself mandate transporting a 488 KiB image. FW1/FW2 are expected
to fit the **existing 256 KiB transport limit**, which stays in
place for now. Exact application marker, image header and CRC
are **Detailed Design**, not already frozen by an existing smoke test.

## 3. Current Boot implementation is NOT aligned yet — P0

A prior Boot v2 proof-of-concept used:

- Signature at `0x6000`, 32-byte image header at `0x6100`,
  and **actual vector table at `0x6800`**.
- A **256 KiB** packed-image/write-transport upper limit;
  **ACCEPTED for current FW1/FW2**, with final image-size verification.
- A common **8 KiB** `0x4000..0x5FFF` config storage assumption
  instead of FW1/FW2's independent 4 KiB regions.

The existing `mcu/AM13E/linker_app_reference.ld` and
`probe_app_linker.py` only exercise this older Boot arrangement.
Their synthetic pass **does not validate** the v1.6 Flash layout,
independent FW configuration
retention, or compatibility with the required fixed
application-entry contract.

The `0x6000` APP_BASE versus `0x6800` vector arrangement requires
an **explicit detailed-design reconciliation** across the ESCape32
packer, Boot validation/jump and Application linker/startup.
Do not silently adopt either arrangement based on a smoke ELF.

The current `_cfg = 0x4000` in the old linker can be a FW1
parameter source, but **must be bounded to 4 KiB for FW1** and
must not erase/write FW2's `0x5000..0x5FFF`. FW2 must use
its own dedicated source/address/commit policy. No extra common
config subregion is allowed without an architecture revision.

**Until that reconciliation is implemented, the existing linker
reference is NOT authorized as the production image format.**

## 4. Rel17 runtime and MCU startup compatibility

- Original Rel17 motor-control/housekeeping/command code stays
  in `src/*.c` and drives the AM13E peripheral adaptation.
- Device startup and CMSIS exception vector names are bridged
  by the target-specific `mcu/AM13E/irq_vectors.c`.
- The Boot reference disables PRIMASK before handoff; runtime
  safe interrupt enabling remains a required unimplemented
  AM13E board service. Do not bypass the safety barrier.
- Link `.data` Flash LMA -> SRAM VMA, `.bss` clear,
  writable `.cfg`, M33 initial stack/Reset_Handler, and
  `.TI.ramfunc` Flash LMA -> SRAM_C VMA using actual
  startup/DriverLib when the agreed production image
  format is defined.
- The current GCC 15.2.1 non-flashable **synthetic** fixture
  correctly places the actual TI
  `DL_FRI_setReadWaitStates()` in SRAM_C RAMFUNC,
  with separate Flash load data; this is verified evidence
  for the DriverLib *section mechanism*, not for the final
  Rel17 image.
- 13/13 libc candidates exist in the selected GNU toolchain
  archives, but complete production link success is not asserted.

## 5. Next development order

1. Keep `add_target(AM13E AM13E)` and rebuild all original
   Rel17 Application objects after the CMake source update.
2. Keep **256 KiB transport** and verify FW1/FW2 binary sizes.
   Reconcile 4+4 KiB firmware parameters and the exact APP_BASE /
   vector / marker semantics across ESCape32 + Boot reference.
3. Validate a production linker **against that agreed contract**,
   not by simply increasing the legacy linker region length.
4. Implement board-specific hardware backends using the **HW
   Baseline pin/peripheral map** (MCPWM0, CMPSS BEMF paths,
   PB14 RX/BiDShot, ADC, safe ENABLE/nFAULT).
5. Carry out real startup/ISR/motor validation only after
   hardware and power-stage safety requirements are met.

The current 11/11 Object Compile PASS, 46 cross-object resolved,
60 undefined and real-FRI synthetic linker test remain valid,
but do not override any outstanding architecture mismatch.
