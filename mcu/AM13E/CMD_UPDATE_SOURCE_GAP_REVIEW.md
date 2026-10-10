# AM13E23019 — ESCape32 Rel17 CMD_UPDATE Source Gap Review

**Status: BOUNDED SRAM STAGING IMPLEMENTED / BOOT COMMIT NOT IMPLEMENTED.**
**Active profile:** Rel17 v1.4, `am13e-port-v2`. The real Boot command
receives CRC-checked blocks and sends original per-block `RES_OK` but
**always ends with final `RES_ERROR` — NO BANK0 ERASE/PROGRAM/RESET**.
This is not silicon, recovery or full Boot Self-update evidence.

## Evidence and authority

1. Original ESCape32 Rel17 `boot/src/main.c`, `boot/src/io.c` and
   `boot/src/util.c` (upstream source is functional authority).
2. Current `boot/src/main.c`, `boot/mcu/AM13E/flash.c`,
   `boot/mcu/AM13E/config.ld`, `boot/src/common.h`,
   `boot/mcu/AM13E/device.c`, `boot/tests/am13e_protocol_host_test.c`.
3. TI AM13E230x TRM SPRUJF2B (August 2026 revision):
   §13.2 Flash banks, §13.3 Flash Controller (especially §13.3.1),
   §5.4 BCR / Fast Boot, §5.5 ROM BSL, §13.4 write protection.
4. Bundled AM13E SDK DriverLib:
   `source/driverlib/am13e230x/dl_flash.c` and FlashCTL implementation.

The original code uses `__attribute__((section(".ramtext")))`
on `write()` and `update()`. The AM13E Boot currently uses
`.TI.ramfunc` and locates the SDK Flash API in SRAM_C.
**Section placement of selected symbols is not proof of the full
self-update call chain or safe Boot image replacement.**

## Exact upstream CMD_UPDATE wire behavior

The original Rel17 parser uses command ID 4 without a new image
header. Its reception loop is bounded by
`n = (_rom_end - _rom) >> 10` (1 KiB blocks in the Boot allocation):

1. `recvdata(buf + pos)` receives an original complement-coded count,
   4-byte-aligned payload (4..1024 bytes) and CRC32.
2. If frame validation fails, the command aborts through the original
   `done` path. There is no success ACK for that frame.
3. A valid block gets `RES_OK`, then `pos += len`.
4. A short block terminates reception. Otherwise reception continues
   until the Boot allocation's maximum block count.
5. `update(_rom, buf, pos)` calls `write()`; after successful write,
   `update()` requests system reset and never returns. If the writer
   returns, the dispatcher emits `RES_ERROR`.

These are **observed original-source semantics**, not new AM13E
implementation claims. The existing wire-frame CRC does not define
an APP whole-image CRC or atomic image-commit marker.

The original STM32 memory aliases `_rom`, `_rom_end`, `_ram_end`
cannot be mechanically reused on AM13E. The selected AM13E Boot
allocation is 16 KiB (`0x0000..0x3fff`), distinct from the
**488 KiB maximum APP Flash allocation** at
`0x6000..0x7ffff`. A future Boot transfer must use the actual
linked Boot binary size within its own 16 KiB allocation, not
synthesize a fixed 488 KiB firmware image.

## AM13E facts and implementation blockers

| Gate | Evidence / needed acceptance |
| --- | --- |
| Physical partition | Boot `0x0000..0x3fff` is Bank0 MAIN; do not touch Cfg `0x4000..0x4fff`, Reserved `0x5000..0x5fff` or APP `0x6000..`. Bank0 MAIN extends beyond the Boot partition, so never use whole-bank erase. |
| Receive staging | Reserve/check up to 16 KiB staged Boot bytes in SRAM_S; test 4-byte alignment, 1 KiB/full/short frame bounds, repeat/abort semantics, stack/heap/data overlap, and malformed-frame CRC handling. SRAM capacity alone does not prove buffer ownership. |
| Boot binary | Generate linked Boot ELF-derived raw bytes with vectors at `0x0000`. Validate actual length, target-specific Boot initial MSP and Thumb Reset PC, erased padding policy, and complete final Flash verification. Do not invent a v1.6 APP header/Signature-last requirement. |
| Flash geometry | TI 2 KiB sector erase and 16-byte ECC program words; bounded 8-sector Boot range. Confirm alignment/tail handling, source in SRAM, protection state and failure diagnostics. |
| Execution safety | TRM §13.3.1: during a same-bank operation, the command-execute/wait path must run in SRAM or the other bank. For self-update, the entire post-first-erase execution/verification/reset path must also avoid returning into replaced Boot Flash, including literal pools, static callees, veneers and interrupt vectors. |
| Interrupts/reset | Qualify IRQ masking, VTOR and exception behavior, watchdog and brown-out interactions; successful Flash verify must hand off to a qualified software reset and post-reset protocol outcome. |
| Recovery | Flash/Reset interruption may corrupt the first Boot vector. On-device ROM BSL/SWD recovery must be demonstrated for the actual NONMAIN BCR/BSL configuration, password/security/lifecycle state and available board pins. |
| Write-protection | `CMD_SETWRP=5` remains a separate mandatory gap. Unreviewed temporary dynamic protection changes do not replace persistent NONMAIN policy or proof that Boot can be legitimately updated. |

**CRITICAL POWER-FAIL BLOCKER:** No atomic Boot swap, rollback or
qualified recovery has been demonstrated. Erasing Bank0 Boot may make
normal boot impossible if power or Flash programming fails. The
existence of TI ROM BSL is **not** a universal rescue guarantee:
TRM §5.4.2 states Fast Boot may disable pin-invoked BSL entry;
§5.5 states BSL may be disabled through NONMAIN, and access is subject
to security/configuration. Do not turn an offline Host PASS into a
recovery or field-update PASS.

TI ROM BCR has optional application CRC capabilities; that is a
separate ROM configuration mechanism, **not** the retired v1.6
ESCape32 APP embedded CRC/header requirement and not a new v1.4
Boot validity gate. The active FW1 launch remains
`Cfg.id=0x32EA` plus M33 APP vectors at `APP_BASE=0x6000`.

## Current source/CI state

- `boot/src/main.c`: AM13E `CMD_UPDATE` uses the original
  `recvdata()`/per-frame `RES_OK` for 4..1024B, max 16 frames,
  short-last terminates. SRAM is cleared after completion; **final
  `RES_ERROR`**, no Bank0 self-programming or success reset.
- `boot/mcu/AM13E/update_staging.c`: real aligned 16KiB SRAM_S
  buffer with ordered/capacity/4-byte checks, abort zeroization,
  basic M33 reset-vector plausibility (not authenticity/completeness).
- `boot/mcu/AM13E/flash.c`: implements **APP-only CMD_WRITE**
  with 1 KiB logical / 2 KiB SRAM RMW, not Boot self-update.
- `boot/mcu/AM13E/update_commit.c`: **quarantined** Boot-only
  eight-sector (2 KiB each) Erase/Program/byte-Verify executor.
  Firmware includes it in SRAM_C but the actual `CMD_UPDATE` parser
  **does not call it**. Its non-returning SRAM wrapper resets only on
  successful physical commit and never falls back into erased Bank0
  Flash; that path is source-/link-tested, NOT authorized on silicon.
  Unused staged bytes are padded to erased `0xff`, without adding
  an APP or Boot image header.
- `boot/tests/am13e_protocol_host_test.c`: tests original CRC
  per-block ACK for short-last/full 16KiB, corrupted CRC and count
  resynchronization, final NAK, three `CMD_SETWRP` modes still NAK,
  and Boot/Cfg/Reserved unchanged.
- `boot/tests/am13e_update_staging_test.c`: tests 16KiB, 17th
  frame, misaligned/duplicate/out-of-order frames, 4B/1020B short
  tails, abort/clear and Cortex-M33 Boot vector plausibility.
- `boot/tests/am13e_update_commit_test.c`: executes the *real*
  sector algorithm against Host Flash mocks, checking full/short
  image, all eight Boot sectors, Erase/Program failures, byte
  corruption/readback and unmodified Cfg/Reserved/APP ranges.
- `verify_rel17_image.py`: checks actual ARM ELF SRAM_C location
  for the executor and non-returning wrapper, plus direct branch
  targets in the RAM-resident command path. This does **not** prove
  indirect/literal accesses or power-loss recovery.
- `verify_v14_source_scope.py`: rejects unreviewed self-update
  activation while keeping the existing source gap explicit.
- `verify_v14_docs.py`: requires this review and missing-operation
  status. Its success only proves static source/document agreement.

## Safe implementation sequence (future work, not completed here)

1. **DONE, source/Host only:** 16KiB bounded SRAM staging and
   original receive/ACK/short-last CRC wire framing.
2. **DONE, Host Flash mock only:** isolated Boot-sector erase,
   program, verify and negative fault injection; no `CMD_UPDATE`
   caller. Real linked Boot BIN staging/negative fixtures and
   protected-device behavior still require dedicated acceptance.
3. **PARTIAL, ARM ELF static evidence:** executor, direct call
   targets and non-returning wrapper checked in SRAM_C. Inspect
   disassembly, literal pools, indirect references, startup copy,
   ISR/NMI, stack, watchdog and brown-out behavior; qualify on silicon.
4. **BLOCKED ON HARDWARE:** establish and physically test ROM
   BSL/SWD recovery under exact NONMAIN and board conditions,
   including interruption after the first vector-sector erase.
5. Only after recovery qualification, connect Boot-only
   Erase/Program/Verify/Reset to original `CMD_UPDATE` and validate
   the original success-reset/no-final-ACK semantics on AM13E.

**Acceptance wording:** BOUNDED SRAM STAGING and Host-mocked,
SRAM-linked Boot-sector Flash Executor IMPLEMENTED; full live
`CMD_UPDATE` Boot Flash Commit / Reset still NOT IMPLEMENTED;
NO BANK0 ERASE FROM THE ACTIVE BOOT COMMAND. Host/ARM CI is NOT
hardware qualification.
