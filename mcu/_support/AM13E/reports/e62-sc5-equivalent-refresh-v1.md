# Experimental SC5 equivalent-reference refresh — v1

Status: isolated experimental acquisition fix; NOT production-approved, NOT Original Rel17.3, NOT P1-qualified. Parent diagnostic branch `diag/e62-sc5-acquisition` is based on source `53953c04f7d857f3b402b3a19870605ed0471d8f`. No physics, DAC calibration constants, reference age limit, or acceptance threshold changed. The feature requires an explicit SIM_ONLY profile and is off by default.

## Causal evidence

The ADC reference producer continues to acquire valid samples while the consumer retains the identity of the sample selected at commutation. In `baseline-fault.json`, sample 1487 was selected at tick 23792, published with active/expected DACH 2047, then faulted at tick 23825 (age 33, existing maximum 32) despite latest sample 1488 being valid. Event 5 records the existing fail-closed entry before reset. No CAP1 was accepted. An uninstrumented source-53953c control also accepts zero captures: the journal is not required to reproduce the failure. DACH 2047 is the result of the explicitly calibrated virtual ADC transfer, never a production seed; no board calibration is claimed.

This is an acquisition-liveness integration defect under the static virtual SC5 profile: a fresh, same-epoch reference with the same computed phase DAC code is not associated with the already published comparator image. The fail-closed policy correctly detects stale evidence; weakening that policy is not the fix.

## Smallest supported correction

`AM13E_EXPERIMENTAL_SC5_EQUIVALENT_REFRESH` services the fresh reference immediately after ADC conversion. A new same-phase/same-epoch sample with an identical computed DAC code can replace an unpublished association, or quarantine an active association until a native MCPWM ZERO republishes it. Active DAC readback must still match. Capture remains disabled throughout quarantine. An already accepted crossing awaiting delayed commutation stays disarmed; CAP1, the accepted-crossing epoch and TIMG12 are not reset or manufactured. Idle/abort/new phase cancels pending refresh. Changed codes, invalid/stale samples, epoch mismatches and incorrect routes retain existing fail-closed handling.

General live changed-code reference tracking is deliberately unresolved. This candidate is not a complete varying-bus calibration implementation. A bounded journal retains only the first 64 and last 256 records; missing middle history is not evidence of invariant satisfaction.

## Experiments, including failures

Raw artifacts live in `escape32-qemu/artifacts/mission-sc5/`; per-case hashes are in `ab-summary.json`. All are explicitly experimental source-built identities, not 05563b69 Original or c49a7485 Production Root Fixes. The QEMU baseline executable is unchanged. Earlier failed candidates are retained.

| Case | Frames | Accepted CAP1 | Outcome |
|---|---:|---:|---|
| uninstrumented-control | 3300 | 0 | fault-reset loop |
| baseline-fault | 3300 | 0 | stale staged sample, valid latest sample |
| tick-only refresh | 3300 | 0 | producer/SysTick ordering still fails |
| producer-triggered refresh | 3300 | 2 | disarmed delayed-commutation interval still fails |
| continuous-equivalent-refresh | 3300 | 9 | new-phase handover still fails |
| quarantined-refresh | 3300 | 97 | unpublished old-sample association still fails |
| pending-association-refresh | 5100 | 264 | 265 scheduled, 263 serviced; wiring smoke passes |
| fixed-no-journal | 3300 | 132 | 132 scheduled, 131 serviced; wiring smoke passes |

The final full run reports 4881 valid-frame feeds, throttle/step zero at end and CPU back in main WFI, whereas the matched baseline remains in fault reset with zero accepted captures. The unchanged P1 analyzer still FAILS the full candidate: experimental identity is not Original, and significant direction reversal persists in both demands (262 direction changes each). Captures do not imply correct six-step alignment, stable acceleration or successful flying restart.

## Reproduction and regression

Use ARM GNU 14.2.1 20241119 and official SDK `b872469ceef18de5e39a1d3acb0821ec158b44c7`. Set `E62_QEMU_ROOT` to the escape32-qemu checkout containing the virtual fixture. Example from the firmware repository:

```sh
cmake -S . -B ../escape32-qemu/.build/mission-fw-sc5-refresh \
  -DAM13E_SDK_ROOT=/tmp/e62-issues/sdk \
  -DE62_QEMU_ROOT=/workspace/escape32-qemu \
  -DAM13E_BOARD_BOARD_PROFILE_FILE="$PWD/mcu/_support/AM13E/cmake/test_profiles/e62_sc5_equivalent_refresh.cmake" \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,-z,max-page-size=0x1000
cmake --build ../escape32-qemu/.build/mission-fw-sc5-refresh --target AM13E
AM13E_SDK_ROOT=/tmp/e62-issues/sdk bash mcu/_support/AM13E/tools/run_sc5_equivalent_refresh_regression.sh
```

The equivalent `_no_journal.cmake` profile removes only the reference journal. Native test (from escape32-qemu; do not use an Original qualification label):

```sh
elf=.build/mission-fw-sc5-fixed-no-journal/AM13E-rev17.3.elf
python3 tests/e62_autonomous_native_motor_probe.py \
  --qemu .build/qemu-src/build/qemu-system-arm --elf "$elf" \
  --experimental-candidate-sha256 "$(sha256sum "$elf" | cut -d' ' -f1)" \
  --cfg-flash artifacts/mission-sc5/sc5-g13b3-cfg-flash.bin \
  --scenario scenarios/e62-native-p0-direct20.json --sim-motor-profile ecx87-kv147 \
  --sim-native-adc-bemf --differential-sync-start --wall-timeout-s 300 \
  --artifact artifacts/mission-sc5/fixed-no-journal.json
```

Official SDK register regressions cover all six floating-phase/polarity mappings and both directions, delayed active-DAC publication, missing/invalid/stale/wrong-phase/epoch mismatches and acquisition rejection. Added tests cover unpublished association refresh, ZERO-only active publication, capture quarantine, unchanged DAC/CAP1/counter/TIMG12, and preserving disarmed state after accepted capture. Full native regression (107 groups) and register suite pass with the experimental macro injected only into SC5 fixtures. Default ARM build passes with the feature off and no new reference observer/service symbols. Integration raw logs, ELF identities and failures remain in the companion evidence archive.

## Unresolved gates

- Changed-code tracking/settling and dynamic bus transients need an explicit source-backed integration design and negative tests; do not widen the age limit.
- The latest accepted crossing/commutation cadence remains inconsistent with stable motor rotation. Issue #5 is independently FAILED, not resolved by this correction.
- Physical transfer calibration, settling and actual route/polarity confirmation need traceable board measurements. SIM_ONLY results do not approve hardware.
- Original Rel17.3 remains unchanged. Review is required before promotion; no automatic merge or issue closure.
