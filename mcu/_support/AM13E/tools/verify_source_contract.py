#!/usr/bin/env python3
"""Current Rev1.6 integration checks against original Rel17 source.
A PASS is audit-traceability, not full source/hardware implementation.
"""
import hashlib
import json
from pathlib import Path
import re
import sys
import argparse

ROOT=Path(__file__).resolve().parents[4]
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
    host=(LOG/"host-tests.log").read_text()
    for test in ("command_decode","command_bidir_roundtrip",
                 "bidir_timing","bemf_commutation","motor_ownership",
                 "service_handoff","servo_source","servo_trim_source",
                 "cli_entry","park_source","aux_wrong_port","aux_serial_conflict",
                 "config_command_source","receiver_config_default",
                 "receiver_config_serial","receiver_config_disabled",
                 "led_source","led_inverted_source","led_four_source",
                 "led_wrong_port","led_serial_conflict","led_erpm_conflict",
                 "aux_led_conflict","motor_source"):
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
    stage=src("boot/mcu/AM13E/hal/update_staging.c")
    check("boot_am13e_stage_begin();" in am13e_update and
          "recvdata((char *)dst)" in am13e_update and
          "boot_am13e_stage_accept(" in am13e_update and
          "sendval(RES_OK);" in am13e_update and
          "boot_am13e_stage_abort();" in am13e_update and
          "#if defined(AM13E_BOOT_RECOVERY_QUALIFIED)" in am13e_update and
          "sendval(RES_ERROR);" in am13e_update and
          "DL_Flash_" not in am13e_update and
          "update(" not in am13e_update and
          "write(" not in am13e_update and
          "DL_Flash_eraseSector(" not in stage and
          "DL_Flash_program(" not in stage and
          "boot/mcu/AM13E/hal/update_staging.c" in
          src("boot/mcu/AM13E/config.cmake") and
          "sendval(RES_ERROR);" in protection,
          "Bounded SRAM staging/Bank0 Flash exclusion changed")

    am13e_write=boot.split("case CMD_WRITE:")[1].split(
        "case CMD_WINDOW:")[0].split("#if defined(AM13E)",1)[1].split(
        "#else",1)[0]
    check("sendval(write(write_addr, buf, len)" in am13e_write and
          "int AM13E_WRITE_ENTRY(" in src("boot/mcu/AM13E/hal/flash.c"),
          "AM13E CMD_WRITE must use original Rel17 write() entry")

    commit=src("boot/mcu/AM13E/hal/update_commit.c")
    boot_linker=src("boot/mcu/AM13E/config.ld")
    check("boot_am13e_commit_sectors(" in commit and
          "DL_Flash_eraseSector(" in commit and
          "DL_Flash_program(" in commit and
          "boot_am13e_update_commit_quarantined(" in commit and
          "NVIC_SystemReset();" in commit and
          "KEEP (*(.TI.ramfunc .TI.ramfunc.*))" in boot_linker and
          "boot_am13e_update_commit_quarantined();" in am13e_update and
          "boot_am13e_commit_sectors" not in boot and
          "update_commit.c" in src("boot/mcu/AM13E/config.cmake"),
          "Boot sector executor must be SRAM-resident and QUARANTINED")
    wrp=src("boot/mcu/AM13E/hal/wrp_nonmain_runtime.c")
    check("AM13E_BOOT_ROM_BSL_SWD_RECOVERY_TESTED" in wrp and
          "AM13E_BOOT_NONMAIN_REVERSIBILITY_TESTED" in wrp and
          "AM13E_BOOT_POWERLOSS_RECOVERY_TESTED" in wrp and
          "AM13E_BOOT_WEP_BITMAP_READBACK_TESTED" in wrp and
          "DL_FlashCTL_programMemory128WithECCGenerated(" in wrp and
          "void setwrp(int type)" in wrp and
          "requested==0x33" in protection and
          "sendval(RES_ERROR);" in protection,
          "Original setwrp must remain Recovery-gated and NAK until qualified")
    # Original CRSF executor/CRC, not replacement command policy;
    # the real UART TX gate must refuse a busy queue *before* Write.
    crsf_app=src("src/io.c")
    uart=src("mcu/AM13E/hal/serial_uart_rx.c")
    crsf_envelope=crsf_app
    check("execcrsfcmd(buf+2,a-1,res+2)" in crsf_app and
          "crsfresponse((const char *)frame,frame[1],scratch)" in crsf_app and
          "crsf_tx.count==0U" in uart and
          "SCB->ICSR=SCB_ICSR_PENDSVSET_Msk" in uart and
          "am13e_crsf_deferred_begin(" in uart and
          "am13e_app_serial_crsf_service_pending(" in uart and
          "am13e_app_serial_crsf_service_pending();" in
              src("mcu/AM13E/hal/irq_vectors.c") and
          "am13e_app_io_crsf_reply(" not in
              uart.split("void RX_HANDLER(void)\n{")[1] and
          "DL_UART_IIDX_EOT_DONE" in uart and
          "am13e_serial_tx_queue_eot(" in uart and
          "AM13E_BOARD_SERIAL_TX_TWO_WIRE==1U" in uart and
          "min(execcrsfcmd(buf+2,a-1,res+2),61)" in crsf_envelope and
          "src/io.c" in src("mcu/AM13E/config.cmake"),
          "Original CRSF stateful command TX must be gated, bounded and board-selected")
    check("void am13e_rel17_boot_main(void)" in boot and
          "void main(void)" in boot and
          "void am13e_rel17_app_main(void)" in src("src/main.c") and
          "void main(void)" in src("src/main.c") and
          "int main(void)" in src("boot/mcu/AM13E/config.c") and
          "int main(void)" in src("mcu/AM13E/config.c"),
          "Native Startup entry bridge must preserve Rel17 void main behavior")

    partition=src("mcu/AM13E/hal/flash_partition.h")
    ld=src("mcu/AM13E/config.ld")
    boot_ld=src("boot/mcu/AM13E/config.ld")
    check("ASSERT(__boot_flash_end__ == 0x00004000" in boot_ld and
          "__app_flash_start__ = 0x00006000" in boot_ld,
          "Native Boot config.ld partition contract mismatch")
    check("AM13E_FLASH_RESERVED_BASE" in partition and
          "AM13E_FLASH_FW2_PARAM_BASE" not in partition and
          "FLASH_RESERVED" in ld,"Rev1.6 Reserved partition missing")

    rx=src("mcu/AM13E/hal/command_capture.c")
    route=src("mcu/AM13E/hal/command_input_reference.h")
    target=src("mcu/AM13E/config.h")
    board=src("mcu/AM13E/hal/board_io.h")
    check("COMMAND_PINCM IO_PINCM" in route and
          "IO_PINCM AM13E_COMMAND_PINCM" in target and "AM13E_COMMAND_PINCM 26U" in board and
          "COMMAND_CAPTURE_FUNCTION IOTIM_CAPTURE_FUNCTION" in route and
          "IOTIM_CAPTURE_FUNCTION IOMUX_PA26_TIMG4_0_CCP0" in target and
          "DL_TIMER_CC_MODE_CAPTURE" in rx and "DL_TIMER_CC_CCOND_TRIG_EDGE" in rx and
          "IOTIM_DMA_TRIGGER" in rx and "DL_DMA_TRIGGER_SOURCE_TIMG4_0_GEN_EVENT1" in target and
          "COUNTERREGS.CC_01[0]" in rx and "ECAP0" not in rx and
          "AM13E_COMMAND_PINCM" in src("boot/mcu/AM13E/hal/io.c") and
          "#define BOOT_IO_PORT AM13E_COMMAND_GPIO" in src("boot/mcu/AM13E/hal/io.c"),
          "Application/Boot must share native PA26; no command ECAP0 fallback")
    isr=rx.split("static void drain_captured(void)")[1]
    check(isr.index("am13e_pb14_bidir_tx_start(") <
          isr.index("am13e_pb14_decoder_pulse("),
          "Rel17 inverted DShot reply must be queued before CRC")
    dma=src("mcu/AM13E/hal/command_reply.c").split("void DMA0_IRQHandler(void)")[1]
    check("am13e_app_io_bidir_telemetry_levels(" in dma,
          "NEXT BiDShot payload must be prepared at TX DMA completion")
    decoder=src("mcu/AM13E/hal/command_decode.c")
    check("pwm(source_ticks,source_period)" in decoder and
          "pulse_us>=28U && pulse_us<=32U" in crsf_app and
          "if(ertm)return;" in crsf_app and
          "servo_calibration_blocked=!!cfg.throt_cal;" in crsf_app,
          "Servo source policy/period or explicit clock-trim blocker missing")
    check("am13e_app_serial_cli_entry_poll(" in rx and
          "am13e_cli_entry_tick(" in uart and
          "am13e_app_pb14_service_takeover()" in uart and
          "!am13e_app_io_service_idle()" in uart,
          "Conditional HIGH CLI entry must be reachable and idle/capture gated")
    reset=src("mcu/AM13E/hal/service_reset.c")
    check(reset.index("am13e_app_motor_fault_shutdown();") <
          reset.index("NVIC_SystemReset();") and
          "am13e_app_service_reset();" in uart,
          "Service reset must shut down native motor before system reset")
    app_source=src("src/main.c")
    check("int y=am13e_app_park_level();" in app_source and
          "am13e_app_erpm_write(0)" in app_source and
          "PARK_PIN needs explicit AM13E" in src("mcu/AM13E/hal/aux_gpio.c"),
          "Conditional PARK/ERPM require real native source hooks/routing guards")
    check("AM13E_PB14_RX_ONESHOT" in decoder and
          "1200000U" in src("mcu/AM13E/hal/bidir_timing.c") and
          "rx_mode=AM13E_PB14_RX_UNDECIDED" not in
          decoder.split("void am13e_pb14_decoder_idle(")[1],
          "Missing Oneshot125/DShot1200 or upstream receiver mode semantics")
    gate=src("boot/mcu/AM13E/hal/app.c")
    check("boot_am13e_quiesce();" in gate and
          "boot_am13e_release_io();" in src("boot/mcu/AM13E/hal/handoff.c") and
          "TMU_LVF_INT_IRQn" in src("boot/mcu/AM13E/hal/handoff.c"),
          "Boot handoff must release IO and clear only implemented IRQ state")
    check('"${REPO_ROOT}/boot/mcu/AM13E/hal/crc.c"' in
          src("mcu/_support/AM13E/tests/boot/am13e_host/CMakeLists.txt") and
          "uint32_t crc32(" not in src("mcu/_support/AM13E/tests/boot/am13e_protocol_host_test.c"),
          "Boot protocol tests must exercise production CRC adapter")
    gate_impl=src("boot/mcu/AM13E/hal/app_validity.c")
    flash=src("boot/mcu/AM13E/hal/flash.c")
    build=src("CMakeLists.txt")
    platform=src("mcu/_support/AM13E/cmake/ti.cmake")
    packer=src("mcu/_support/AM13E/tools/pack_am13e_rel17.py")
    check("boot_am13e_app_validity(" in gate and
          "AM13E_BOOT_CFG_ID" in gate_impl and
          ".id = 0x32ea" in src("src/main.c"),
          "Missing original Rel17 Cfg.id/APP vector boot gate")
    check("boot_am13e_image_check(" not in gate and
          "image_integrity.h" not in gate and
          "AM13E_IMAGE_SIGNATURE_OFFSET" not in flash and
          "boot_am13e_image_check(" not in flash,
          "Retired manifest-based APP CRC/signature boot check reintroduced")
    check(".signature" not in ld and ".image_header" not in ld and
          "LENGTH = 0x0007A000" in ld,
          "APP linker must expose maximum 488KiB with no metadata slots")
    check("function(add_target name mcu)" in build and
          "am13e_configure_target(${elf})" in build and
          "CMAKE_CURRENT_SOURCE_DIR}/mcu/AM13E/config.ld" in platform and
          "pack_am13e_rel17.py" in platform and
          "REL17_V14" not in build,
          "Native build must use one shared add_target and current image contract")
    check("MAX_LENGTH=APP_END-APP_BASE" in packer and
          "TRANSPORT_ALIGN=4" in packer and
          "image_crc32" not in packer and
          "SIGNATURE_OFFSET" not in packer,
          "Flat packer must use actual linked size, without embedded CRC")
    check("#if AM13E_MOTOR_OUTPUT_CONFIGURED" in
          src("mcu/AM13E/hal/motor_power_stage.c") and
          "Motor outputs require explicit" in
          src("mcu/AM13E/hal/motor_output_configuration.h"),
          "Physical motor enable requires explicit electrical board definitions")

    print("PASS current Rel17/Rev1.6 source integration and fail-closed safety checks")
    print("LIMIT: source checks do not establish complete runtime or hardware parity")

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--logs",type=Path,default=LOG)
    args=parser.parse_args()
    LOG=args.logs
    try:main()
    except (OSError,RuntimeError,ValueError,KeyError) as exc:
        sys.exit("FAIL current source contract: "+str(exc))
