# Isolated changed-code SC5 handover experiment — 2026-11-07 (Asia/Taipei)

Status: REGISTER_TESTED experimental acquisition extension, not production calibration, motor qualification or hardware approval. Branch `experiment/e62-sc5-changed-handover` is stacked on `experiment/e62-sc5-equivalent-refresh` (Firmware Draft PR7, itself stacked on PR6). Production/default branch is `am13e-port-v2`. Original05563, Production Root Fixesc49a7 and exact filter5b90 remain separate identities and untouched.

## Reproduced missing behavior

Commit2eff4a4 adds a register-level reproduction to the existing official-SDK SC5 fixture. An acquired, coherent ADC sample produces a changed calibrated phase reference while capture is active. The equivalent-only consumer keeps the prior sample/code. With a newer sample still valid, the staged sample exceeds the unchanged32-tick age limit and enters the real fail-closed shutdown/reset path. This is the live changed-code acquisition gap; it does not imply the safe-off policy is wrong or establish a board defect. The baseline test remains available when the changed-code feature is off.

## Isolated correction

Opt-in `AM13E_EXPERIMENTAL_SC5_CHANGED_REFRESH` requires the existing equivalent-refresh feature, SC5 calibration and an explicit SIM_ONLY profile with `AM13E_EXPERIMENTAL_SC5_SETTLE_ZEROS`. No raw DAC target is seeded or hardcoded. New phase codes come only from the existing calibrated ADC conversion.

A fresh same-epoch changed reference quarantines capture before writing the DAC shadow. Publication waits for native MCPWM ZERO and actual active-DAC readback; an old active code can wait only within the original staged sample's32-tick deadline. Fresh producer updates cannot extend this pending handover indefinitely. An unexpected active code fails closed. Once active load is observed, the comparator filter is initialized while capture remains disabled, followed by the explicit number of complete ZERO intervals. Before publication, latest reference validity, epoch, selected-phase code, CMPSS phase mux, ECAP input and edge polarity must still agree. Changed-again/invalid/epoch/route failures do not authorize capture.

The previous accepted-crossing epoch, CAP1 register, free-running counter and TIMG12 schedule are not reset by handover. If an accepted edge is waiting for delayed commutation, publication remains disarmed. A new phase or abort cancels the old pending handover. Motor IRQ priority0 precedes ADC priority1, matching the existing native service ordering; no host Guest-MMIO or synthetic feedback is introduced into an actual QEMU run.

The fixture chooses **two** settling ZERO intervals as an explicit synthetic policy, not a measured analog settling constant. The register fixture does not emulate comparator analog settling; these tests demonstrate quarantine/order, not settling sufficiency. General rapidly changing-code tracking is intentionally fail-closed, not silently followed. Default builds and PR7's equivalent-only profile are unchanged.

## Executed regressions

```sh
source /workspace/setup-escape32/activate.sh
AM13E_SDK_ROOT=/tmp/e62-issues/sdk bash mcu/_support/AM13E/tools/run_sc5_equivalent_refresh_regression.sh
AM13E_SDK_ROOT=/tmp/e62-issues/sdk bash mcu/_support/AM13E/tools/run_sc5_changed_handover_regression.sh
cmake -S /workspace/ESCape32 -B /workspace/escape32-qemu/.build/mission-fw-sc5-changed-no-journal -G Ninja \
  -DESCAPE32_BUILD_AM13E=ON -DAM13E_SDK_ROOT=/tmp/e62-issues/sdk \
  -DE62_QEMU_ROOT=/workspace/escape32-qemu -DG13B2_SIM_ONLY_OPT_IN=ON \
  -DG13B2_SIM_ONLY_IDEAL_BEMF_REFERENCE=OFF \
  -DAM13E_BOARD_BOARD_PROFILE_FILE=/workspace/ESCape32/mcu/_support/AM13E/cmake/test_profiles/e62_sc5_changed_handover_no_journal.cmake \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,-z,max-page-size=0x1000
cmake --build /workspace/escape32-qemu/.build/mission-fw-sc5-changed-no-journal --target AM13E -j3
```

Baseline:107 host groups plus full SDK register suite PASS, including the new expected-failure liveness reproduction. Candidate:107 groups/full register suite PASS, including changed-code load delay/settling, both change directions, timeout despite fresh producer updates, invalid conversion, unexpected active DAC, changed-again publication, epoch invalidation, wrong mux/input/polarity, preserving accepted TIMG12 wait, phase replacement and stop cancellation. Original missing/stale/calibration/phase/polarity regressions are retained. These are explicitly synthetic **UNIT register stimuli**, not fabricated motor measurements or raw SITL evidence.

ARM candidate and feature-off builds link PASS. SDK is b872469ceef18de5e39a1d3acb0821ec158b44c7; ARM GCC14.2.1 20241119. Source759a558b0714547b45fffc56b6a76ebd25efa575; no-reference-journal ELF SHA `d7a44dcda4f836ec6da15b19398af092cc79ff102b54778b0cd58bb245f9cba6`. Instrumented ELF SHA `a800f3af711dcc6cb2d905f34a2a301a171a037a95e0656e878da5b454e7ac89` is a separate build, not mixed into the no-journal regression.

The separate `escape32-qemu` branch `experiment/e62-sc5-changed-regression` pins the no-journal ELF and runs six unchanged nominal scenarios/eight existing analog stresses. It preserves all strict failures, including Original identity rejection. Results are recorded after completion. No result substitutes for Issue3's exact5b90 qualification.

## Unresolved acceptance

- A source-owned changing-VBUS/native-ADC SITL stimulus is not demonstrated by the existing constant-VBUS scenario. Constant-input motor runs cannot prove the changed-code path was exercised. Register tests do not close this integration gate.
- Filter/analog settling sufficiency, bus slew/ripple envelope and board transfer measurements remain unqualified. Do not infer production values from these fixtures.
- New code arriving during a pending handover fails closed. A general tracking/cancellation policy needs independent requirements and tests.
- Startup/restart and offset robustness remain independent motor gates. Preserving capture counters or compiling the code is not a motor PASS.
- Review before any promotion. No automatic merge, production change or issue closure.
