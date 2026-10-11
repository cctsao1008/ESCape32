# AM13E Rel17 Rev1.4 — Conditional MCU Adapter Gaps

Source-defined modes remain conditional as in upstream ESCape32 Rel17.
Missing support for a selected mode is a **PORTING GAP**, not an
approved exclusion. See [V14_DESIGN_AUTHORITY.md](V14_DESIGN_AUTHORITY.md).

| Original Rel17 feature family | Current AM13E reference evidence / gap |
| --- | --- |
| Servo PWM, Oneshot125, DShot300/600/1200 | Host-tested PB14 reference adapters; on-silicon timing not qualified |
| BiDShot/extended telemetry | Host-tested GCR/NRZI and reply sequence; physical turnaround and board direction drive untested |
| Analog receiver `input_mode=1` | ADC0 third-SOC sample and raw-to-mV conversion now reach original Rel17 `adcdata(...,a)` under explicit `ANALOG_CHAN` Board Profile; Native Host conversion and ARM conditional syntax gates added. **Reference has NO assigned receiver pin/channel, so no physical analog receiver is enabled or qualified.** |
| Serial/iBUS/SBUS/SBUS2/CRSF/EXBUS/HoTT receiver | iBUS input_mode=3: frame checksum/channel decoder now Host-tested and connected to original Rel17 setthrot()/setbrake(); **physical UART RX pinmux unassigned on Reference; optional TI UNICOMM IRQ/FIFO implementation requires board UC/RX pin/function/clock plus explicit SBUS inversion setting (DMA not yet implemented)**. SBUS: original 25B / 16x11bit receive and throttle/brake conversion now Host-tested under input_mode=4, but 8E2/inversion and SBUS2 slot TX pending. CRSF input_mode=5: CRC8-DVB-S2, 416666 baud and 16x11-bit channel callback now Host/ARM implemented; 0x28/0x2c/0x2d command replies now use original execcrsfcmd() in PendSV, with bounded IRQ-to-PendSV mailbox and UART TX FIFO/EOT under explicit two-wire Board Profile; still no physical route validation or single-wire half-duplex/telemetry scheduling. EXBUS input_mode=6: original crc16ccitt + little-endian >>3 channel RX and telemetry-request identification implemented at 125000 baud; original 125/250 kbaud adaptive switching and half-duplex telemetry TX pending. HoTT/SUMD input_mode=7: original crc16xmodem, SUMDv1/v3 mapping and 115200 baud RX implemented; physical frame-idle synchronization and on-silicon UART timing remain pending. Original Serial mode=2 still pending |
| KISS/iBUS/S.Port/CRSF/MSB/HoTT telemetry | Original formatters retained; AM13E selected serial TX provider pending |
| Sine/Brushed/Hall/hybrid modes | Internal motor software tested in part; conditional Hall/board adapter pending |
| BEC/LED/ERPM/PARK/beacon | Existing original guards retained; selected board GPIO implementations pending |
| Current/voltage/temperature | Original `SENS_MAP`/board-dependent channels; reference NTC/VBUS numeric model tested, current route unqualified |
| Boot self-update and write-protection | `CMD_UPDATE=4` now receives original framed blocks into bounded 16KiB SRAM with per-block ACK; **final `RES_ERROR`** remains. An isolated SRAM_C Boot-sector writer passes Host-mock and ARM static checks, but is not callable from the dispatcher pending physical recovery qualification. `CMD_SETWRP=5` still returns `RES_ERROR`. Both remain mandatory gaps |
| Six-step physical power stage | Pads/gate physically disabled in Reference; no motor-drive hardware approval |

The Rev1.4 mapping's `hal_motor.c`, `hal_input.c`, etc. are
ownership examples for MCU-local hardware adaptation. They are
not proof of compulsory filename spellings or 26 mandatory wrapper
functions. Resolve equivalence through existing ESCape32 hooks and
actual source behavior; do not introduce a parallel architecture.

**Status: PARTIAL SOURCE/ARM/HOST EVIDENCE; FULL REL17 PORT NOT COMPLETE.**
