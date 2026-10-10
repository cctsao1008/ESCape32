#!/usr/bin/env python3
"""Rev1.4 upstream source contract checker: pass with EXPLICIT blockers.
A PASS is audit-traceability, not full source/hardware implementation.
"""
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[3]
LOG=ROOT/"am13e-ci-logs"
BASE="c3907601c174b8a422ce5ac4f20e6c6f8302cc51800be1ad6a92b5d599abcdd8"

def check(value, reason):
    if not value: raise RuntimeError(reason)

def src(path):
    p=ROOT/path
    check(p.is_file(), "Missing source: "+path)
    return p.read_text()

def main():
    check(hashlib.sha256((ROOT/"src/prog.c").read_bytes()).hexdigest()==BASE,
          "Rel17 src/prog.c changed: review command API before claiming parity")
    feature_sources={
        "src/main.c":("nextstep(","compctl(","HALL_MAP","SINE_RANGE",
                      "BRUSHED","PARK_PIN","ERPM_PIN","adcdata(", "PWM_ENABLE"),
        "src/io.c":("entryirq(","calibirq(","servoirq(","dshotirq(",
                    "iotim_dma_isr(","dshotcrc(","serialirq(","ibusfunc(",
                    "sbusfunc(","crsffunc(","exbusfunc(","hottfunc("),
        "src/telem.c":("sendtelem(","sendtelemdata(","sendkiss(",
                       "sendcrsf(","dshotval"),
        "src/util.c":("savecfg(","resetcfg(","BEC_MAP","LED_MAP"),
        "src/defs.h":("SENS_MAP","LED_MAP"),
    }
    for path,symbols in feature_sources.items():
        content=src(path)
        for symbol in symbols:
            check(symbol in content, "Original Rel17 feature absent: "+
                  path+"/"+symbol)
    host=src("am13e-ci-logs/host-tests.log")
    for test in ("command_decode","command_bidir_roundtrip",
                 "bidir_timing","bemf_commutation","motor_ownership"):
        check(re.search(r"(?m)^PASS: "+test+r"\s*$",host),
              "Missing Native regression PASS: "+test)

    boot=src("boot/src/main.c")
    for num,name in enumerate(("CMD_PROBE","CMD_INFO","CMD_READ",
                                "CMD_WRITE","CMD_UPDATE","CMD_SETWRP",
                                "CMD_WINDOW")):
        check(re.search(r"#define\s+"+name+r"\s+"+str(num)+r"\b",boot),
              "Rel17 original command ID changed: "+name)
    update=boot.split("case CMD_UPDATE:")[1].split("case CMD_SETWRP:")[0]
    protection=boot.split("case CMD_SETWRP:")[1].split("default:")[0]
    check("sendval(RES_ERROR);" in update and
          "sendval(RES_ERROR);" in protection,
          "Boot self-update/WRP state changed; update the v1.4 conformance audit")

    partition=src("mcu/AM13E/flash_partition.h")
    ld=src("mcu/AM13E/linker_app_v16.ld")
    check("AM13E_FLASH_RESERVED_BASE" in partition and
          "AM13E_FLASH_FW2_PARAM_BASE" not in partition and
          "FLASH_RESERVED" in ld,"Rev1.4 Reserved partition missing")

    rx=src("mcu/AM13E/command_capture.c")
    isr=rx.split("void ECAP0_IRQHandler(void)")[1]
    check(isr.index("am13e_pb14_bidir_tx_start(") <
          isr.index("am13e_pb14_decoder_pulse("),
          "Rel17 inverted DShot reply must be queued before CRC")
    dma=src("mcu/AM13E/command_reply.c").split("void DMA0_IRQHandler(void)")[1]
    check("am13e_app_io_bidir_telemetry_levels(" in dma,
          "NEXT BiDShot payload must be prepared at TX DMA completion")
    decoder=src("mcu/AM13E/command_decode.c")
    check("AM13E_PB14_RX_ONESHOT" in decoder and
          "1200000U" in src("mcu/AM13E/bidir_timing.c") and
          "rx_mode=AM13E_PB14_RX_UNDECIDED" not in
          decoder.split("void am13e_pb14_decoder_idle(")[1],
          "Missing Oneshot125/DShot1200 or upstream receiver mode semantics")
    check("boot_am13e_app_validity(" in src("boot/mcu/AM13E/app.c") and
          "AM13E_BOOT_CFG_ID" in src("boot/mcu/AM13E/app_validity.h"),
          "Missing approved Rel17 Cfg.id/vector Boot gate")
    check("am13e_power_stage_attach(" not in
          src("mcu/AM13E/motor_power_stage.c"),
          "Five user-deferred IO-only features silently enabled")

    result={
        "design_revision":"1.4",
        "result":"PASS_SOURCE_AUDIT_WITH_EXPLICIT_BLOCKERS",
        "upstream_original_command_parser_byte_identical":True,
        "original_boot_host_tested":[0,1,2,3],
        "am13e_address_extension_host_tested":6,
        "original_boot_missing":["CMD_UPDATE","CMD_SETWRP"],
        "conditional_adapters_pending":[
            "input_mode_analog","input_mode_serial",
            "UART_telemetry","Hall_hybrid","LED_BEC_PARK_ERPM",
            "current_limit_board_io_only"],
        "image_abi_conflict":"Original Cfg.id marker vs retained v1.6 CRC/header/signature-last",
        "io_only":["Gate Enable","nFAULT","Independent OC",
                   "Serial Telemetry TX","Current Limiting"],
        "physical_validation":"PENDING"
    }
    LOG.mkdir(exist_ok=True,parents=True)
    (LOG/"rel17-v14-source-audit.json").write_text(
        json.dumps(result,indent=2)+"\n")
    print("PASS: Rev1.4 original Rel17 source scope audited (BLOCKERS documented)")
    print("NOT COMPLETE: CMD_UPDATE, CMD_SETWRP, conditional board adapters")
    print("OPEN CONFLICT: Original Cfg.id vs v1.6 Image CRC/signature ABI")

if __name__=="__main__":
    try: main()
    except (OSError,RuntimeError,ValueError,IndexError) as exc:
        sys.exit("FAIL v1.4 scope audit: "+str(exc))
