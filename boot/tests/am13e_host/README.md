# AM13E Host Validation — ESCape32 Rel17 v1.4 (CURRENT)

**Status:** executable native-GCC/ARM software evidence only; not silicon or motor-drive qualification.
**Authority:** [AM13E Boot/Application Contract](../../../mcu/AM13E/APP_LINK_CONTRACT.md),
[Rel17 v1.4 Source Gap Audit](../../../mcu/AM13E/REL17_V14_SOURCE_GAP_AUDIT.md),
original ESCape32 Rel17 source, and the active build/linker/Boot implementation.
**Historical v1.6 tests:** [README_V16_SUPERSEDED.md](README_V16_SUPERSEDED.md).
Those older header/CRC/signature-last instructions are **SUPERSEDED**, not current test or update requirements.

## Active Boot / Application contract

| Address range | Allocation / ownership |
| --- | --- |
| `0x0000..0x3fff` | 16 KiB ESCape32-derived Boot |
| `0x4000..0x4fff` | 4 KiB persistent Cfg; first halfword `Cfg.id=0x32EA` |
| `0x5000..0x5fff` | Reserved 4 KiB; no APP writer |
| `0x6000..0x7ffff` | **488 KiB maximum Flash allocation**, one Application |

`APP_BASE=0x00006000`. Cortex-M33 initial MSP and Thumb Reset PC are
at APP+0x000 and APP+0x004. Boot checks **persistent `Cfg.id=0x32EA`
plus plausible M33 application vectors** before VTOR/handoff.

The **actual firmware image length comes from the linked ARM ELF / objcopy
binary**, not a mandatory 488 KiB file. The transport `.flat.bin` is
the real linked BIN plus only 0–3 bytes of `0xFF` for four-byte alignment.
The separate JSON sidecar reports actual size/SHA256 for host reference;
Boot does not parse that JSON.

There is **no active APP image header, whole-image CRC, APP signature,
or Signature-last completion/commit gate**. Wire-frame CRC remains active
for the original ESCape32 Boot protocol. An incomplete APP rewrite may
leave a valid Cfg/vector combination: **software CI does not prove
power-loss-safe update, complete firmware installation, or rollback**.

## Real Boot wire protocol and host coverage

Original command IDs remain `CMD_PROBE=0`, `CMD_INFO=1`,
`CMD_READ=2`, `CMD_WRITE=3`, `CMD_UPDATE=4`,
`CMD_SETWRP=5`; AM13E adds `CMD_WINDOW=6`.

- Frames retain complement-encoded value bytes and the original
  per-command CRC32; `CMD_WRITE` uses 1 KiB logical blocks.
- TI Flash has 2 KiB erase sectors; the writer performs SRAM
  read-modify-write and readback to preserve the adjacent 1 KiB half.
  Partial final logical blocks are supported.
- `CMD_WINDOW=6` selects window 0 or 1;
  `effective_block = 256*window + block`. Blocks 0–487 cover the
  allocated 488 KiB APP region; block 488 is rejected.
- APP commands cannot write Boot/Cfg/Reserved regions.
- **`CMD_UPDATE` and `CMD_SETWRP` are not implemented.** Both
  currently return `RES_ERROR`; recognition of a command ID is
  not feature parity. See the mandatory blockers in the source gap audit.

The test suite uses the production Boot parser/Flash and validity
implementations at an isolated native-GCC mock boundary. It tests
malformed framing/CRC, retries, random/duplicate/short writes,
sector RMW, variable actual image lengths, window 0/1, the last
physical APP block, and Cfg/vector plausibility. The simulated
488 KiB boundary fixture proves addressing capacity; it does
**not** define a fixed firmware length.

## Reproduce active tests

The full GitHub Actions workflow
[`am13e-fw1-compile-link.yml`](../../../.github/workflows/am13e-fw1-compile-link.yml)
pins a TI SDK revision, compiles and strictly links the original
Rel17-derived full FW1, produces actual `AM13E_FW1_REL17.elf`,
`AM13E_FW1_REL17.bin`, `AM13E_FW1_REL17.flat.bin` and
`AM13E_FW1_REL17.json`, and links `BOOT5_PB14.elf`.
It also runs full Host CTest, image/linker audits, functional
coverage, original-source gap checks and documentation synchronization.

After a successful ARM build in `build-am13e`, from the repository root:

```bash
# Prerequisite: the ARM build has already produced
# build-am13e/AM13E_FW1_REL17.flat.bin
cmake -S boot/tests/am13e_host -B build-am13e-host-tests \
  -DAM13E_HOST_SANITIZERS=OFF \
  -DAM13E_HOST_PACKED_IMAGE="$PWD/build-am13e/AM13E_FW1_REL17.flat.bin"
cmake --build build-am13e-host-tests -j"$(nproc)"
ctest --test-dir build-am13e-host-tests --output-on-failure -V
python3 mcu/AM13E/tools/verify_v14_docs.py
```

Without `AM13E_HOST_PACKED_IMAGE`, base CTest tests still run.
When the actual file is given, the optional linked-image transaction,
real protocol, 257 KiB window-1 fixture and full 488 KiB addressing
fixture are also exercised. The fixtures are **Host tests**, not
released images or proof of target Flash endurance.

The separate opt-in `AM13E_APP_SMOKE` is a minimal vector-first
link/packer *fixture*. It is **not** production motor-control FW1.
The historical `pack_am13e_v2.py` filename remains as an implementation
compatibility alias currently containing v1.4 flat-image logic;
the supported full FW1 profile selects `pack_am13e_rel17.py`.
Only `AM13E_IMAGE_PROFILE=REL17_V14` is supported for linked FW1;
`NONE` is for object-only builds with the full image disabled.

## Acceptance limits

ARM strict link, native Host CTest, wire-frame CRC/Flash mock,
static vector layout and source/document audit **do not prove**
AM13E hardware Boot/Flash behavior, CRCP register equivalence,
physical PB14 timing, live WiFi-Link updater compatibility,
Flash ECC and same-bank execution, brown-out recovery, or motor
operation. The current Reference build does not activate unqualified physical
Gate/OC/nFAULT/UART/current functions. They are **implementation
and board-qualification gaps**, not permanent Rel17 feature exclusions
under the supplied Integration Design Rev1.4.

Never report this suite as **Hardware Validation PASS**.
