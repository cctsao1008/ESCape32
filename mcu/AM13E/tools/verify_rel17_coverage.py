#!/usr/bin/env python3
"""Executable Rel17/AM13E software-coverage *evidence* audit.

A PASS proves each declared ported software module is in the real ARM
application build and that its declared host regression ran successfully.
It CANNOT prove silicon waveforms, power-stage operation or WWDT reset.
The five user-deferred features must remain IO-only.
"""
import argparse
import json
from pathlib import Path
import re
import sys

def require(condition, description):
    if not condition:
        raise RuntimeError(description)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--build",required=True,type=Path)
    parser.add_argument("--logs",required=True,type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[3]
    data=json.loads((root/"mcu/AM13E/REL17_FUNCTIONAL_COVERAGE.json").read_text())
    require(data["schema_version"]==1,"Unsupported feature audit schema")
    groups=("ported_software","io_only")
    all_entries=[(scope,e) for scope in groups for e in data[scope]]
    ids=[e["id"] for _,e in all_entries]
    require(len(ids)==len(set(ids)) and len(ids)>=15,
            "Feature coverage manifest missing/duplicate IDs")
    for scope in groups:
        require(data[scope],f"Empty coverage group: {scope}")

    commands=json.loads((args.build/"compile_commands.json").read_text())
    app=set()
    for item in commands:
        if "CMakeFiles/AM13E.dir/" not in item["command"]:
            continue
        path=Path(item["file"]).resolve()
        try:
            app.add(str(path.relative_to(root.resolve())))
        except ValueError:
            # TI DriverLib is compiled from the pinned external SDK,
            # but is not an ESCape32-owned feature source.
            continue
    require(len(app)>=45,"Real AM13E Rel17 source compilation missing")
    summary=(args.logs/"summary.txt").read_text()
    for key in ("CONFIGURE_RC=0","OBJECT_COMPILE_RC=0","STRICT_LINK_RC=0"):
        require(key in summary,"ARM FW1 build/link gate failed: "+key)
    for path in ("AM13E_FW1_REL17.elf","AM13E_FW1_REL17.flat.bin",
                 "boot/BOOT5_PB14.elf"):
        require((args.build/path).stat().st_size>0,
                "Real Rel17 v1.4 image/boot artifact missing: "+path)
    for log in ("rel17-host-test.log","boot-host-test.log"):
        require("100% tests passed" in (args.logs/log).read_text(),
                "Rel17 v1.4 pack/Boot transaction host test failed: "+log)

    host=(args.logs/"host-tests.log").read_text()
    report=[]
    all_tests=set()
    for scope,item in all_entries:
        sid=item["id"]
        sources=item["sources"]
        tests=item["host_tests"]
        require(sources and tests,"Unbacked feature: "+sid)
        for src in sources:
            require((root/src).is_file(),"Missing feature source "+src)
            require(src in app,"Feature omitted from linked FW1 "+sid+
                                ": "+src)
        for case in tests:
            require(re.search(r"(?m)^PASS: "+re.escape(case)+r"\s*$",host)
                    is not None,
                    "Declared native regression not PASS "+sid+"/"+case)
            all_tests.add(case)
        report.append({"id":sid,"label":item["label"],"scope":scope,
                       "arm_compiled_and_linked":True,
                       "host_regressions_passed":tests,
                       "physical_verified":False})

    # These tests must use the IMPLEMENTED runtime functions. A
    # disconnected test-only model is not an acceptable coverage claim.
    required_symbols={
        "mcu/AM13E/command_capture.c":[
            "am13e_pb14_capture_snapshot_valid(",
            "am13e_pb14_decoder_abort("],
        "mcu/AM13E/command_reply.c":[
            "am13e_bidir_turnaround_ticks("],
        "mcu/AM13E/motor_bemf.c":[
            "am13e_bemf_sample_plan(",
            "am13e_app_motor_on_bemf_event("],
        "src/main.c":["am13e_bemf_policy_plan(",
                      "am13e_app_motor_bemf_commutation_delay_us("],
        "mcu/AM13E/motor_safety.c":[
            "am13e_motor_owner_next(",
            "AM13E_OWNER_EVENT_SIXSTEP",
            "AM13E_OWNER_EVENT_SINE",
            "AM13E_OWNER_EVENT_BRAKE",
            "AM13E_OWNER_EVENT_MUSIC_START",
            "AM13E_OWNER_EVENT_PCM_START",
            "AM13E_OWNER_EVENT_AUDIO_END"],
        "mcu/AM13E/motor_power_stage.c":["gate_hold_inactive()"]
    }
    for src,needles in required_symbols.items():
        code=(root/src).read_text()
        for symbol in needles:
            require(symbol in code,"Runtime integration missing "+src+
                                   " : "+symbol)

    cmake=(root/"CMakeLists.txt").read_text()
    stage=(root/"mcu/AM13E/motor_power_stage.c").read_text()
    fault=(root/"mcu/AM13E/fault_input.c").read_text()
    vectors=(root/"mcu/AM13E/irq_vectors.c").read_text()
    motor=(root/"mcu/AM13E/motor_safety.c").read_text()
    cfg=(root/"mcu/AM13E/board_configuration.h").read_text()
    for item in ("mcu/AM13E/motor_fault_route.c",
                 "mcu/AM13E/fault_trip_backend.c",
                 "mcu/AM13E/telem_uc2_preflight.c"):
        require(item not in app,"Excluded physical protection/UART linked "+item)
    require("AM13E_BOARD_POWER_STAGE_PROFILE=1" not in cmake,
            "IO-only build silently enables hardware power stage")
    require("am13e_power_stage_attach(" not in motor and
            "am13e_power_stage_attach(" not in stage and
            "DL_XBAR_selectPWMXBARSource(" not in stage,
            "Physical gate attach or OC trip activated")
    require("DL_GPIO_enableInterrupt(" not in fault and
            "DL_GPIO_disableInterrupt(" in fault and
            "am13e_app_nfault_asserted(" not in vectors,
            "PB15 fault interrupt/runtime behavior is NOT IO-only")
    require("DL_GPIO_initDigitalOutput(IOMUX_PINCM_PB13)" in stage,
            "PB13 inactive-only output mode not initialized")
    for pin in ("OC_GPIO","SERIAL_TX","CURRENT_SENSE"):
        require(re.search(r"#define AM13E_BOARD_"+pin+r"_PINCM 0\b",cfg),
                "Unassigned optional aux pin became a fabricated pin: "+pin)
    require("DL_GPIO_initPeripheralAnalogFunction(AM13E_BOARD_CURRENT_SENSE_PINCM)" in fault,
            "Current Sense optional pin initialization lost")

    result={
        "baseline":data["baseline"],
        "profile":data["branch"],
        "status":"PASS_SOFTWARE_EVIDENCE_ONLY",
        "firmware_arm_strict_link":"PASS",
        "boot_rel17_image_contract":"PASS",
        "feature_records":report,
        "ported_software_count":len(data["ported_software"]),
        "io_only_count":len(data["io_only"]),
        "distinct_native_regressions":len(all_tests),
        "hardware_pending":data["hardware_pending"],
        "outside_fw1":data["outside_fw1"],
        "qualification":data["evidence_limits"]
    }
    args.logs.mkdir(exist_ok=True,parents=True)
    (args.logs/"rel17-functional-coverage.json").write_text(
        json.dumps(result,indent=2,ensure_ascii=False)+"\n")
    lines=["# Rel17 / AM13E FW1 — Executable Software Evidence",
           "",
           "ARM FW1 strict link: **PASS**; Boot/Image Rel17 v1.4: **PASS**.",
           "",
           "| Feature | Scope | ARM linked | Host regression |",
           "| --- | --- | --- | --- |"]
    for record in report:
        lines.append(f"| {record['label']} | {record['scope']} | PASS | "+
                     ", ".join(record["host_regressions_passed"])+" |")
    lines.extend(["", "## Explicitly NOT verified", ""])
    lines.extend("- "+p for p in data["hardware_pending"])
    (args.logs/"rel17-functional-coverage.md").write_text(
        "\n".join(lines)+"\n")
    print(f"PASS: {len(data['ported_software'])} ported-software features, "+
          f"{len(data['io_only'])} IO-only, "+
          f"{len(all_tests)} distinct native cases, FW1/Boot Rel17 v1.4 link")
    print("SCOPE: physical hardware timing, ADC, gate and motor spin NOT verified")
    return 0

if __name__=="__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, ValueError, KeyError) as exc:
        sys.exit("FAIL: Rel17 functional coverage: "+str(exc))
