# AM13E23019 — ESCape32 Rel17 CMD_UPDATE Source Gap Review

**Status: SOURCE-LEVEL REVIEW / NOT IMPLEMENTED.**
**Active profile:** Rel17 v1.4, `am13e-port-v2`. This is not silicon,
power-fail, recovery or released update evidence. Boot `CMD_UPDATE=4`
must continue to return `RES_ERROR`; no Boot Bank0 Erase/Program
is enabled by this review.

## Evidence and authority

1. Original ESCape32 Rel17 `boot/src/main.c`, `boot/src/io.c` and
   `boot/src/util.c` (upstream source is functional authority).
2. Current `boot/src/main.c`, `boot/mcu/AM13E/flash.c`,
   `boot/mcu/AM13E/linker_boot_reference.ld`, `boot/src/common.h`,
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

- `boot/src/main.c`: AM13E `CMD_UPDATE` immediately returns
  `RES_ERROR`, before consuming data or touching Bank0. This is
  deliberate fail-closed **NOT IMPLEMENTED** behavior.
- `boot/mcu/AM13E/flash.c`: implements **APP-only CMD_WRITE**
  with 1 KiB logical / 2 KiB SRAM RMW, not Boot self-update.
- `boot/tests/am13e_protocol_host_test.c`: asserts
  `CMD_UPDATE` NAK, command resynchronization, all three
  `CMD_SETWRP` selectors still NAK, and Boot/Cfg/Reserved unchanged.
- `verify_v14_source_scope.py`: rejects unreviewed self-update
  activation while keeping the existing source gap explicit.
- `verify_v14_docs.py`: requires this review and missing-operation
  status. Its success only proves static source/document agreement.

## Safe implementation sequence (future work, not completed here)

1. Freeze a testable 16 KiB max Boot transfer contract preserving
   upstream command framing and short-last-block semantics.
2. Produce real ELF-derived Boot BIN and negative fixtures:
   empty/oversized/incomplete input, bad Frame CRC, bad Boot vectors,
   overrun, power interruption at each sector, protected target.
3. Inspect the real Boot ELF/map/disassembly to establish complete
   SRAM-resident post-first-erase execution, with startup copy and
   stack/interrupt/watchdog handling proven on silicon.
4. Establish and physically test ROM BSL/SWD recovery under
   target NONMAIN settings before enabling Bank0 erase.
5. Only then integrate Boot-only Erase/Program/Verify/Reset and
   validate host success/retry/recovery behavior on an AM13E board.

**Acceptance wording:** SOURCE GAP REVIEWED; `CMD_UPDATE` still
NOT IMPLEMENTED; Boot Bank0 programming DISABLED; Host/ARM CI
is NOT hardware qualification.
