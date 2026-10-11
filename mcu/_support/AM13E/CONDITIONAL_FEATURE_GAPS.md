# AM13E Rel17 Rev1.6 — Conditional MCU Adapter Gaps

Source-defined modes remain conditional as in upstream ESCape32 Rel17.
Missing support for a selected mode is a **PORTING GAP**, not an
approved exclusion. See [DESIGN_AUTHORITY.md](DESIGN_AUTHORITY.md).

| Original Rel17 feature family | Current AM13E reference evidence / gap |
| --- | --- |
| Servo PWM, Oneshot125, DShot300/600/1200 | Host-tested PB14 reference adapters; on-silicon timing not qualified |
| BiDShot/extended telemetry | Host-tested GCR/NRZI and reply sequence; physical turnaround and board direction drive untested |
| Analog receiver `input_mode=1` | ADC0 third-SOC sample and raw-to-mV conversion now reach original Rel17 `adcdata(...,a)` under explicit `ANALOG_CHAN` Board Profile; Native Host conversion and ARM conditional syntax gates added. **Reference has NO assigned receiver pin/channel, so no physical analog receiver is enabled or qualified.** |
| Serial/iBUS/SBUS/SBUS2/CRSF/EXBUS/HoTT receiver | **Conditional Source/Host/ARM:** original Serial mode2 (460800 baud), iBUS3, SBUS4, CRSF5, EXBUS6, HoTT/SUMD7 with native TI UNICOMM IRQ/FIFO. CRSF commands deferred to PendSV; EXBUS n=10 125/250k retry through guarded PendSV, original 7-description/38B reply format and conditional two-wire TX. SBUS2 slot-ID/packet/timing **plan** implemented, NOT real timed TX. Reference has NO qualified UART physical route. Single-wire half-duplex, DMA, 8E2/inversion and silicon timing remain pending |
| KISS/iBUS/S.Port/CRSF/MSB/HoTT telemetry | Original `src/telem.c` formatters and request handling + optional board-selected **two-wire** UNICOMM IRQ/FIFO/EOT, 128-byte pending buffer segmented into 64-byte TX transactions; synthetic ARM link/Host regression PASS. Missing real one-wire PHY/turnaround, S.Port gap, HoTT timing and electrical qualification. No Reference UART enabled |
| Sine/Brushed/Hall/hybrid modes | Internal motor software tested in part; conditional Hall/board adapter pending |
| BEC/LED/ERPM/PARK/beacon | Existing original guards retained; selected board GPIO implementations pending |
| Current/voltage/temperature | Original `SENS_MAP`/board-dependent channels; reference NTC/VBUS numeric model tested, current route unqualified |
| Boot self-update and write-protection | `CMD_UPDATE=4`: bounded original CRC/ACK reception and Recovery-gated SRAM_C eight-sector erase/program/verify/reset. `CMD_SETWRP=5`: conditional NONMAIN WRP Off/Boot/Full for **both Bank0 and Bank1**, preserves BCR and reserved bits, guarded by ROM BSL/SWD, reversibility, power-loss and WEP Bitmap readback. **Default Reference: final RES_ERROR for both, zero destructive Boot/NONMAIN operations.** Positive CI fixture NEVER FLASH; hardware recovery NOT qualified |
| Six-step physical power stage | Pads/gate physically disabled in Reference; no motor-drive hardware approval |

The Rev1.6 mapping's `hal_motor.c`, `hal_input.c`, etc. are
ownership examples for MCU-local hardware adaptation. They are
not proof of compulsory filename spellings or 26 mandatory wrapper
functions. Resolve equivalence through existing ESCape32 hooks and
actual source behavior; do not introduce a parallel architecture.

**Status: PARTIAL SOURCE/ARM/HOST EVIDENCE; FULL REL17 PORT NOT COMPLETE.**
