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
    # Original Rel17 reception restored, but no Bank0 Boot Flash commit.
    am13e_update=update.split("#if defined(AM13E)",1)[1].split("#else",1)[0]
    stage=src("boot/mcu/AM13E/update_staging.c")
    check("boot_am13e_stage_begin();" in am13e_update and
          "recvdata((char *)dst)" in am13e_update and
          "boot_am13e_stage_accept(" in am13e_update and
          "sendval(RES_OK);" in am13e_update and
          "boot_am13e_stage_abort();" in am13e_update and
          "sendval(RES_ERROR);" in am13e_update and
          "DL_Flash_" not in am13e_update and
          "update(" not in am13e_update and
          "write(" not in am13e_update and
          "DL_Flash_eraseSector(" not in stage and
          "DL_Flash_program(" not in stage and
          "boot/mcu/AM13E/update_staging.c" in
          src("boot/mcu/AM13E/config.cmake") and
          "sendval(RES_ERROR);" in protection,
          "Bounded SRAM staging/Bank0 Flash exclusion changed")

    am13e_write=boot.split("case CMD_WRITE:")[1].split(
        "case CMD_WINDOW:")[0].split("#if defined(AM13E)",1)[1].split(
        "#else",1)[0]
    check("sendval(write(write_addr, buf, len)" in am13e_write and
          "int AM13E_WRITE_ENTRY(" in src("boot/mcu/AM13E/flash.c"),
          "AM13E CMD_WRITE must use original Rel17 write() entry")

    check("void am13e_rel17_boot_main(void)" in boot and
          "void main(void)" in boot and
          "void am13e_rel17_app_main(void)" in src("src/main.c") and
          "void main(void)" in src("src/main.c") and
          "int main(void)" in src("boot/mcu/AM13E/entry.c") and
          "int main(void)" in src("mcu/AM13E/entry.c"),
          "Native Startup entry bridge must preserve Rel17 void main behavior")

    partition=src("mcu/AM13E/flash_partition.h")
    ld=src("mcu/AM13E/config.ld")
    boot_ld=src("boot/mcu/AM13E/config.ld")
    check("ASSERT(__boot_flash_end__ == 0x00004000" in boot_ld and
          "__app_flash_start__ = 0x00006000" in boot_ld,
          "Native Boot config.ld partition contract mismatch")
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
    gate=src("boot/mcu/AM13E/app.c")
    gate_impl=src("boot/mcu/AM13E/app_validity.c")
    flash=src("boot/mcu/AM13E/flash.c")
    build=src("CMakeLists.txt")
    profile=src("mcu/AM13E/profiles/image_rel17_v14.cmake")
    packer=src("boot/tools/pack_am13e_rel17.py")
    check("boot_am13e_app_validity(" in gate and
          "AM13E_BOOT_CFG_ID" in gate_impl and
          ".id = 0x32ea" in src("src/main.c"),
          "Missing original Rel17 Cfg.id/APP vector boot gate")
    check("boot_am13e_image_check(" not in gate and
          "image_integrity.h" not in gate and
          "AM13E_IMAGE_SIGNATURE_OFFSET" not in flash and
          "boot_am13e_image_check(" not in flash,
          "Retired v1.6 APP CRC/signature boot check reintroduced")
    check(".signature" not in ld and ".image_header" not in ld and
          "LENGTH = 0x0007A000" in ld,
          "APP linker must expose maximum 488KiB with no metadata slots")
    check('AM13E_IMAGE_PROFILE "REL17_V14"' in build and
          "image_rel17_v14.cmake" in build and
          "AM13E_FW1_REL17_IMAGE" in build and
          "mcu/AM13E/config.ld" in profile and
          "boot/mcu/AM13E/config.ld" in profile and
          "pack_am13e_rel17.py" in profile,
          "Build must select approved flat Rel17 image profile")
    check("MAX_LENGTH=APP_END-APP_BASE" in packer and
          "TRANSPORT_ALIGN=4" in packer and
          "image_crc32" not in packer and
          "SIGNATURE_OFFSET" not in packer,
          "Flat packer must use actual linked size, without embedded CRC")
    check("am13e_power_stage_attach(" not in
          src("mcu/AM13E/motor_power_stage.c"),
          "Unqualified Reference power-stage attach unexpectedly enabled")

    result={
        "design_revision":"1.4",
        "result":"PASS_SOURCE_AUDIT_WITH_EXPLICIT_BLOCKERS",
        "upstream_original_command_parser_byte_identical":True,
        "original_boot_host_tested":[0,1,2,3],
        "am13e_address_extension_host_tested":6,
        "original_boot_missing":["CMD_UPDATE","CMD_SETWRP"],
        "boot_self_update_preflight":"BOUNDED_SRAM_STAGE_RECEIVE_ONLY_BANK0_ERASE_DISABLED",
        "boot_update_recovery":"ROM_BSL_SWD_NONMAIN_DEPENDENT_NOT_VERIFIED",
        "conditional_adapters_pending":[
            "input_mode_analog","input_mode_serial",
            "UART_telemetry","Hall_hybrid","LED_BEC_PARK_ERPM",
            "current_limit_board_adapter_unqualified"],
        "boot_abi":"RESOLVED_REL17_V14_CFG_ID_AND_APP_VECTOR",
        "app_image":"FLAT_ELF_DERIVED_VARIABLE_SIZE_MAX_488KIB",
        "torn_update_risk":"Valid Cfg plus vectors may boot incomplete later code",
        "reference_unqualified_io":["Gate Enable","nFAULT","Independent OC",
                   "Serial Telemetry TX","Current Limiting"],
        "source_feature_policy":"REL17_CONDITIONAL_FEATURES_MUST_BE_PRESERVED",
        "physical_validation":"PENDING"
    }
    LOG.mkdir(exist_ok=True,parents=True)
    (LOG/"rel17-v14-source-audit.json").write_text(
        json.dumps(result,indent=2)+"\n")
    print("PASS: Rev1.4 original Rel17 source scope audited (BLOCKERS documented)")
    print("NOT COMPLETE: CMD_UPDATE, CMD_SETWRP, conditional board adapters")
    print("ABI RESOLVED: Cfg.id=0x32EA + M33 APP vector; no APP CRC/header")
    print("LIMIT: interrupted APP updates have no whole-image commit gate")

if __name__=="__main__":
    try: main()
    except (OSError,RuntimeError,ValueError,IndexError) as exc:
        sys.exit("FAIL v1.4 scope audit: "+str(exc))
