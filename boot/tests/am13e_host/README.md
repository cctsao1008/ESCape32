# AM13E real-source Host validation

Current authority: original Rel17 source and the three original Rev1.6 documents in mcu/_support/AM13E/v1.6/. These are software tests, not physical Flash or recovery qualification.

Native images are AM13E-rev17.3.{elf,bin,hex} and boot/BOOT5_PB14-rev4.{elf,bin,hex}. Give AM13E_HOST_PACKED_IMAGE the actual Application BIN; tests never substitute a mock firmware image for that input. Configure this directory separately with a native compiler, build and run CTest. With the real image supplied, all 11 CTests must execute and pass. CI also runs unique driver/Core units, selected-profile ARM links, routing/recovery negatives and current image/source/design checks.

Boot occupies0000–3fff, Config4000–4fff, Reserved5000–5fff and Application6000–7ffff (488 KiB maximum allocation, not fixed image size). Wire blocks0–3 map Config,4–491 map Application,492–511 are invalid. APP-only streams start at4 and preserve Config. The packager requires explicit Config data and authorization for combined streams. Reserved is never mapped.

The production parser retains original complement framing, per-frame CRC32, command IDs, ACK/NAK sequencing and write() entry. Logical 1 KiB writes use physical 2 KiB sector RMW/readback. Tests cover short writes, retries, companion-half preservation, malformed frames, invalid ranges/alignment, protected banks, injected erase/program/readback failures and zero Flash calls on rejection.

Boot launch uses persistent Cfg.id=0x32EA and plausible M33 vectors at APP_BASE=0x6000. There is no image header, payload CRC, signature-last gate or mandatory runtime manifest. The JSON sidecar reports linked size/hash only. Partial uploads can leave plausible vectors: software tests do not prove completeness or brownout-safe installation.

CMD_UPDATE receives up to16KiB through original CRC/ACK handling into bounded SRAM. Default unqualified builds finish with RES_ERROR and clear staging; Bank0 commit and CMD_SETWRP remain recovery-gated. Host tests exercise the quarantined sector executor on memory mocks, including failures/corruption and partition preservation. ARM audits check SRAM_C placement and direct branches; neither establishes indirect/literal safety, actual erase-time recovery or NONMAIN reversibility.

The window1/full488 fixtures and tiny-image parser tests preserve unique boundary coverage. Historical manifest experiments, smoke products and alternate image modes are retired; Git history retains them. Physical motor, Flash, Bank0 self-update and NONMAIN/security operations are prohibited during software validation.
