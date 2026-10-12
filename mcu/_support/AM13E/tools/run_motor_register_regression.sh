#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
sdk=${AM13E_SDK_ROOT:?Set AM13E_SDK_ROOT to the TI SDK for production register regressions}
out=$(mktemp -d /tmp/am13e-motor-register.XXXXXX)
trap 'rm -rf "$out"' EXIT
python3 mcu/_support/AM13E/tools/prepare_sdk_sources.py --sdk-root "$sdk" --output "$out"
# Synthetic electrical fixtures only; never a qualified board configuration.
for fixture in high low full inverted blanking; do
polarity=1; mask=0; extra=()
case "$fixture" in
  low) polarity=0; mask=63 ;;
  full) extra=(-DFULL_DUTY) ;;
  inverted) mask=21; extra=(-DINVERTED_HIGH) ;;
  blanking) extra=(-DSW_BLANKING) ;;
esac
hw="$sdk/source/device/am13e230x/include/hw"
dl="$sdk/source/driverlib/am13e230x"
cc "${extra[@]}" -D_GNU_SOURCE -std=c11 -O1 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -DAM13E -DAM13E_REF_IO_PLAN_V1=1 -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' \
 -DAM13E_MOTOR_GATE_ACTIVE_LEVEL="$polarity" -DAM13E_MOTOR_PAD_INVERT_MASK="$mask" -DAM13E_MOTOR_OUTPUT_RED_TICKS=120 -DAM13E_MOTOR_OUTPUT_FED_TICKS=0 -DAM13E_MOTOR_MIN_DEAD_TICKS=100 -DAM13E_MOTOR_NFAULT_ACTIVE_LOW=1 -DAM13E_MOTOR_GATE_RESET_INACTIVE=1 \
 "-DAM13E_TEST_TI_ECAP_HEADER=\"$dl/dl_ecap.h\"" "-DAM13E_TEST_TI_GPIO_HEADER=\"$dl/dl_gpio.h\"" "-DAM13E_TEST_TI_MCPWM_HEADER=\"$dl/dl_mcpwm.h\"" \
 -Isrc -Isrc/serial -Isrc/runtime -Imcu/AM13E -Imcu/AM13E/hal -Imcu/AM13E/hal/tests/register_mock -I"$hw" -I"$dl" \
 mcu/AM13E/hal/tests/motor_physical_host_test.c src/util.c \
 mcu/AM13E/hal/motor_audio.c mcu/AM13E/hal/motor_audio_plan.c mcu/AM13E/hal/motor_safety.c mcu/AM13E/hal/motor_output_runtime.c mcu/AM13E/hal/motor_output_backend.c mcu/AM13E/hal/board_motor_output_reference.c mcu/AM13E/hal/motor_power_stage.c mcu/AM13E/hal/motor_output_route_plan.c \
 mcu/AM13E/hal/motor_fault_route.c mcu/AM13E/hal/fault_trip_backend.c mcu/AM13E/hal/fault_trip_route_plan.c \
 mcu/AM13E/hal/motor_phase_plan.c mcu/AM13E/hal/motor_aq_plan.c mcu/AM13E/hal/motor_shadow_plan.c mcu/AM13E/hal/motor_pwm_shadow_plan.c mcu/AM13E/hal/motor_frequency_plan.c mcu/AM13E/hal/motor_duty_plan.c mcu/AM13E/hal/motor_ownership_plan.c \
 mcu/AM13E/hal/motor_event_timer.c mcu/AM13E/hal/motor_timer_math.c mcu/AM13E/hal/motor_bemf.c mcu/AM13E/hal/motor_bemf_event_plan.c mcu/AM13E/preset.c \
 "$dl/dl_mcpwm.c" "$dl/dl_cmpss_lite.c" "$dl/dl_ecap.c" "$out/dl_timer.c" \
 -o "$out/motor-physical-test"
"$out/motor-physical-test"
done

base=(-DAM13E_MOTOR_GATE_ACTIVE_LEVEL=1 -DAM13E_MOTOR_PAD_INVERT_MASK=0
 -DAM13E_MOTOR_OUTPUT_RED_TICKS=120 -DAM13E_MOTOR_OUTPUT_FED_TICKS=0
 -DAM13E_MOTOR_MIN_DEAD_TICKS=100 -DAM13E_MOTOR_NFAULT_ACTIVE_LOW=1
 -DAM13E_MOTOR_GATE_RESET_INACTIVE=1)
header=mcu/AM13E/hal/motor_output_configuration.h
cc -std=c11 -Werror "${base[@]}" -include "$header" -x c -fsyntax-only /dev/null
for invalid in missing_gate bad_gate bad_pad missing_min short_dead nonzero_fed reset_active pwm_enable inverted_mismatch; do
  case "$invalid" in
    missing_gate) args=(-UAM13E_MOTOR_GATE_ACTIVE_LEVEL) ;;
    bad_gate) args=(-UAM13E_MOTOR_GATE_ACTIVE_LEVEL -DAM13E_MOTOR_GATE_ACTIVE_LEVEL=2) ;;
    bad_pad) args=(-UAM13E_MOTOR_PAD_INVERT_MASK -DAM13E_MOTOR_PAD_INVERT_MASK=64) ;;
    missing_min) args=(-UAM13E_MOTOR_MIN_DEAD_TICKS) ;;
    short_dead) args=(-UAM13E_MOTOR_OUTPUT_RED_TICKS -DAM13E_MOTOR_OUTPUT_RED_TICKS=90) ;;
    nonzero_fed) args=(-UAM13E_MOTOR_OUTPUT_FED_TICKS -DAM13E_MOTOR_OUTPUT_FED_TICKS=20) ;;
    reset_active) args=(-UAM13E_MOTOR_GATE_RESET_INACTIVE -DAM13E_MOTOR_GATE_RESET_INACTIVE=0) ;;
    pwm_enable) args=(-DPWM_ENABLE) ;;
    inverted_mismatch) args=(-DINVERTED_HIGH) ;;
  esac
  if cc -std=c11 -Werror "${base[@]}" "${args[@]}" -include "$header" -x c -fsyntax-only /dev/null >"$out/$invalid.log" 2>&1; then
    echo "FAIL: invalid motor electrical definition accepted: $invalid" >&2; exit 1
  fi
  echo "PASS: rejected motor electrical definition: $invalid"
done
