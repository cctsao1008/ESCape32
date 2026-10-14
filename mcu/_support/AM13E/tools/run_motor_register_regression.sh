#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
sdk=${AM13E_SDK_ROOT:?Set AM13E_SDK_ROOT to the TI SDK for production register regressions}
out=$(mktemp -d /tmp/am13e-motor-register.XXXXXX)
trap 'rm -rf "$out"' EXIT
python3 mcu/_support/AM13E/tools/prepare_sdk_sources.py --sdk-root "$sdk" --output "$out"
# Synthetic electrical fixtures only; never a qualified board configuration.
for fixture in high low full inverted blanking hall pwm_enable pwm_enable_full fatal sc5; do
polarity=1; mask=0; fed=0; extra=(); extra_sources=()
case "$fixture" in
  sc5)
    extra=(-DAM13E_SC5_FIXTURE -include mcu/_support/AM13E/tests/hal/sc5_calibration_fixture.h
      -DAM13E_BOARD_ADC_FULLSCALE=4095 -DAM13E_BOARD_ADC_VREF_MV=2500
      -DAM13E_BOARD_NTC_SUPPLY_MV=3300 -DAM13E_BOARD_NTC_MODEL=3
      -DAM13E_BOARD_VBUS_TOP_OHMS=15000 -DAM13E_BOARD_VBUS_BOTTOM_OHMS=1000)
    extra_sources=(mcu/AM13E/hal/analog_sampling_backend.c
      mcu/AM13E/hal/analog_calibration_plan.c "$sdk/source/driverlib/am13e230x/dl_adc.c") ;;
  low) polarity=0; mask=63 ;;
  full) extra=(-DFULL_DUTY) ;;
  inverted) mask=21; extra=(-DINVERTED_HIGH) ;;
  blanking) extra=(-DSW_BLANKING) ;;
  hall) extra=(-DHALL_MAP=1 -DUSE_XOR -DAM13E_HALL_GPIO_TIMESTAMP=1
    -DAM13E_HALL_PIN1=12 -DAM13E_HALL_PIN2=28 -DAM13E_HALL_PIN3=31
    -DAM13E_HALL_PULL=DL_GPIO_RESISTOR_PULL_UP -DAM13E_HALL_INVERT_MASK=0) ;;
  pwm_enable) fed=120; extra=(-DPWM_ENABLE -DAM13E_MOTOR_PWM_ENABLE_TOPOLOGY=1 -DAM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS=1500) ;;
  pwm_enable_full) fed=120; mask=21; extra=(-DPWM_ENABLE -DAM13E_MOTOR_PWM_ENABLE_TOPOLOGY=1 -DAM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS=1500 -DFULL_DUTY -DINVERTED_HIGH -DSW_BLANKING) ;;
  fatal)
    # Deliberately inconsistent ADC fullscale makes a real scaling failure
    # injectable. Synthetic LED/ERPM/UART definitions, never a board profile.
    extra=(-DAM13E_BOARD_ADC_VREF_MV=3300 -DAM13E_BOARD_NTC_SUPPLY_MV=3300 -DAM13E_BOARD_VBUS_TOP_OHMS=26820 -DAM13E_BOARD_VBUS_BOTTOM_OHMS=1000 -DAM13E_BOARD_NTC_MODEL=3 -DAM13E_FATAL_PATH_FIXTURE -DAM13E_BOARD_ADC_FULLSCALE=2047
      -Wl,--wrap=__error__
      -Wno-unused-parameter -Wno-unused-variable
      -DLED_MAP=0xB0 -DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U
      -DERPM_PIN=1 -DAM13E_BOARD_ERPM_GPIO=GPIO1 -DAM13E_BOARD_ERPM_MASK=2U -DAM13E_BOARD_ERPM_PINCM=33U
      -DAM13E_BOARD_SERIAL_RX_CONFIGURED=1 -DAM13E_BOARD_SERIAL_RX_UC_INDEX=0
      -DAM13E_BOARD_SERIAL_RX_PINCM=26 -DAM13E_BOARD_SERIAL_RX_PIN_FUNCTION=8
      -DAM13E_BOARD_SERIAL_RX_BUSCLK_HZ=100000000 -DAM13E_BOARD_SERIAL_SBUS_PIN_INVERT=1
      -DAM13E_BOARD_SERIAL_TX_CONFIGURED=1 -DAM13E_BOARD_SERIAL_TX_PINCM=26
      -DAM13E_BOARD_SERIAL_TX_PIN_FUNCTION=9 -DAM13E_BOARD_SERIAL_TX_TWO_WIRE=0
      -DAM13E_BOARD_SERIAL_CLI_ENTRY_CONFIGURED=1)
    extra_sources=(src/io.c mcu/AM13E/serial/serial_frame_rx.c mcu/AM13E/serial/cli_line_rx.c
      mcu/AM13E/hal/serial_tx_queue.c mcu/AM13E/hal/crsf_deferred_command.c
      mcu/AM13E/hal/analog_sampling_backend.c mcu/AM13E/hal/analog_calibration_plan.c
      "$sdk/source/driverlib/am13e230x/dl_adc.c"
      "$sdk/source/driverlib/am13e230x/dl_unicomm.c"
      "$sdk/source/driverlib/am13e230x/dl_unicommuart.c") ;;
esac
hw="$sdk/source/device/am13e230x/include/hw"
dl="$sdk/source/driverlib/am13e230x"
cc "${extra[@]}" -D_GNU_SOURCE -std=c11 -O1 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -DAM13E_BOARD_MCU_PART=23019 -DAM13E_BOARD_PACKAGE=1 -DAM13E -DAM13E_REF_IO_PLAN_V1=1 -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' \
 -DAM13E_OC_U_DAC_CODE=2048 -DAM13E_OC_V_DAC_CODE=2048 -DAM13E_OC_W_DAC_CODE=2048 -DAM13E_OC_U_INVERT=0 -DAM13E_OC_V_INVERT=0 -DAM13E_OC_W_INVERT=0 -DAM13E_MOTOR_GATE_ACTIVE_LEVEL="$polarity" -DAM13E_MOTOR_PAD_INVERT_MASK="$mask" -DAM13E_MOTOR_OUTPUT_RED_TICKS=120 -DAM13E_MOTOR_OUTPUT_FED_TICKS="$fed" -DAM13E_MOTOR_MIN_DEAD_TICKS=100 -DAM13E_MOTOR_NFAULT_ACTIVE_LOW=1 -DAM13E_MOTOR_GATE_RESET_INACTIVE=1 \
 "-DAM13E_TEST_TI_ECAP_HEADER=\"$dl/dl_ecap.h\"" "-DAM13E_TEST_TI_GPIO_HEADER=\"$dl/dl_gpio.h\"" "-DAM13E_TEST_TI_MCPWM_HEADER=\"$dl/dl_mcpwm.h\"" \
 "-DAM13E_TEST_TI_EQEP_HEADER=\"$dl/dl_eqep.h\"" \
 "-DAM13E_TEST_TI_UART_HEADER=\"$dl/dl_unicommuart.h\"" \
 -Isrc -Imcu/AM13E/serial -Imcu/AM13E -Imcu/AM13E -Imcu/AM13E/hal -Imcu/_support/AM13E/tests/hal/register_mock -I"$hw" -I"$dl" \
 mcu/_support/AM13E/tests/hal/motor_physical_host_test.c src/util.c \
 mcu/AM13E/hal/motor_hall.c \
 mcu/AM13E/hal/motor_audio.c mcu/AM13E/hal/motor_audio_plan.c mcu/AM13E/hal/motor_safety.c mcu/AM13E/hal/motor_output_runtime.c mcu/AM13E/hal/motor_output_backend.c mcu/AM13E/hal/board_motor_output_reference.c mcu/AM13E/hal/motor_power_stage.c mcu/AM13E/hal/motor_output_route_plan.c \
 mcu/AM13E/hal/motor_fault_route.c mcu/AM13E/hal/fault_trip_backend.c mcu/AM13E/hal/fault_trip_route_plan.c \
 mcu/AM13E/hal/motor_phase_plan.c mcu/AM13E/hal/motor_aq_plan.c mcu/AM13E/hal/motor_shadow_plan.c mcu/AM13E/hal/motor_pwm_shadow_plan.c mcu/AM13E/hal/motor_frequency_plan.c mcu/AM13E/hal/motor_duty_plan.c mcu/AM13E/hal/motor_ownership_plan.c \
 mcu/AM13E/hal/motor_event_timer.c mcu/AM13E/hal/motor_timer_math.c mcu/AM13E/hal/motor_bemf.c mcu/AM13E/hal/motor_bemf_event_plan.c mcu/AM13E/preset.c \
 "$dl/dl_mcpwm.c" "$dl/dl_cmpss_lite.c" "$dl/dl_ecap.c" "$out/dl_timer.c" "${extra_sources[@]}" \
 -o "$out/motor-physical-test"
"$out/motor-physical-test"
done

# A complete physical output implementation requires real asynchronous
# overcurrent contracts; an approval declaration is neither needed nor enough.
if cc -fsyntax-only -std=c11 -DAM13E -DCLK=200000000 \
    -DAM13E_MOTOR_GATE_ACTIVE_LEVEL=1 -DAM13E_MOTOR_PAD_INVERT_MASK=0 \
    -DAM13E_MOTOR_OUTPUT_RED_TICKS=120 -DAM13E_MOTOR_OUTPUT_FED_TICKS=0 \
    -DAM13E_MOTOR_MIN_DEAD_TICKS=100 -DAM13E_MOTOR_NFAULT_ACTIVE_LOW=1 \
    -DAM13E_MOTOR_GATE_RESET_INACTIVE=1 -Wno-int-to-pointer-cast \
    -Isrc -Imcu/AM13E -Imcu/AM13E/hal \
    -Imcu/_support/AM13E/tests/hal/register_mock -I"$hw" -I"$dl" \
    mcu/AM13E/hal/motor_fault_route.c >"$out/missing-oc.log" 2>&1; then
  echo "FAIL: physical outputs accepted missing OC contracts" >&2; exit 1
fi
grep -q 'Configured Option 1 outputs require explicit overcurrent contracts' "$out/missing-oc.log"
echo "PASS: rejected configured physical outputs missing native OC contracts"

# Native SC5 ADC reference/package contracts must be explicit and valid.
for contract in missing_mode invalid_mode qfn_internal; do
  ref_flags=('-DAM13E_BOARD_SC5_CALIBRATION={}' -DAM13E_BOARD_ADC_PACKAGE_QFN48=0)
  case "$contract" in
    missing_mode) expected='SC5 requires explicit native ADC reference mode' ;;
    invalid_mode) ref_flags+=(-DAM13E_BOARD_ADC_REFERENCE_MODE=99); expected='Unsupported native ADC reference mode' ;;
    qfn_internal) ref_flags=(-DAM13E_BOARD_SC5_CALIBRATION={} -DAM13E_BOARD_ADC_PACKAGE_QFN48=1
      -DAM13E_BOARD_ADC_REFERENCE_MODE=DL_SYSCTL_VREF_INTERNAL_2_5_V); expected='QFN48 uses VDDA/VSSA' ;;
  esac
  if cc -fsyntax-only -std=c11 -DAM13E -DCLK=200000000 '-DTARGET_NAME="Host"' \
      -DAM13E_REF_IO_PLAN_V1=1 "${ref_flags[@]}" -Wno-int-to-pointer-cast \
      -Isrc -Imcu/AM13E/serial -Imcu/AM13E -Imcu/AM13E -Imcu/AM13E/hal \
      -Imcu/_support/AM13E/tests/hal/register_mock -I"$hw" -I"$dl" \
      mcu/AM13E/hal/analog_runtime.c >"$out/$contract.log" 2>&1; then
    echo "FAIL: invalid SC5 ADC contract compiled: $contract" >&2; exit 1
  fi
  grep -q "$expected" "$out/$contract.log"
  echo "PASS: rejected SC5 ADC contract: $contract"
done

# Exercise the native PA26 capture/DMA/reply path and original io.c policy.
cc -no-pie -D_GNU_SOURCE -std=c11 -O1 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -include mcu/_support/AM13E/tests/app/newlib_host.h "-DAM13E_TEST_TI_DMA_HEADER=\"$dl/dl_dma.h\"" -DAM13E_BOARD_MCU_PART=23019 -DAM13E_BOARD_PACKAGE=1 -DAM13E -DAM13E_REF_IO_PLAN_V1=1 -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' \
  \
 "-DAM13E_TEST_TI_ECAP_HEADER=\"$dl/dl_ecap.h\"" "-DAM13E_TEST_TI_GPIO_HEADER=\"$dl/dl_gpio.h\"" "-DAM13E_TEST_TI_MCPWM_HEADER=\"$dl/dl_mcpwm.h\"" \
 "-DAM13E_TEST_TI_EQEP_HEADER=\"$dl/dl_eqep.h\"" \
 -Isrc -Imcu/AM13E/serial -Imcu/AM13E -Imcu/AM13E -Imcu/AM13E/hal -Imcu/_support/AM13E/tests/hal/register_mock -I"$hw" -I"$dl" \
 mcu/_support/AM13E/tests/hal/command_pa26_host_test.c src/io.c mcu/_support/AM13E/tests/app/rel17_defaults_host.c \
 mcu/AM13E/hal/command_decode.c mcu/AM13E/hal/bidir_timing.c mcu/AM13E/serial/bidir_codec.c \
 "$out/dl_timer.c" \
 -o "$out/command-pa26-test"
"$out/command-pa26-test"

for serial_baud in 460800 230400; do
cc -DAM13E_BOARD_DEBUG_UART_BAUD=115200 -DSERIAL_BR="$serial_baud" -no-pie -D_GNU_SOURCE -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -include mcu/_support/AM13E/tests/app/newlib_host.h "-DAM13E_TEST_TI_DMA_HEADER=\"$dl/dl_dma.h\"" -DAM13E_BOARD_MCU_PART=23019 -DAM13E_BOARD_PACKAGE=1 -DAM13E -DAM13E_REF_IO_PLAN_V1=1 -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' \
  \
 "-DAM13E_TEST_TI_ECAP_HEADER=\"$dl/dl_ecap.h\"" "-DAM13E_TEST_TI_GPIO_HEADER=\"$dl/dl_gpio.h\"" "-DAM13E_TEST_TI_MCPWM_HEADER=\"$dl/dl_mcpwm.h\"" \
 "-DAM13E_TEST_TI_EQEP_HEADER=\"$dl/dl_eqep.h\"" \
 "-DAM13E_TEST_TI_UART_HEADER=\"$dl/dl_unicommuart.h\"" -DAM13E_BOARD_SERIAL_RX_CONFIGURED=1 -DAM13E_BOARD_SERIAL_RX_UC_INDEX=0 -DAM13E_BOARD_SERIAL_RX_PINCM=26 -DAM13E_BOARD_SERIAL_RX_PIN_FUNCTION=8 -DAM13E_BOARD_SERIAL_RX_BUSCLK_HZ=100000000 -DAM13E_BOARD_SERIAL_SBUS_PIN_INVERT=1 -DAM13E_BOARD_SERIAL_TX_CONFIGURED=1 -DAM13E_BOARD_SERIAL_TX_PINCM=26 -DAM13E_BOARD_SERIAL_TX_PIN_FUNCTION=9 -DAM13E_BOARD_SERIAL_TX_TWO_WIRE=0 -DAM13E_BOARD_SBUS2_TIMED_REPLY=1 -DAM13E_BOARD_SBUS2_TX_PIN_INVERT=1 \
 -Isrc -Imcu/AM13E/serial -Imcu/AM13E -Imcu/AM13E -Imcu/AM13E/hal -Imcu/_support/AM13E/tests/hal/register_mock -I"$hw" -I"$dl" \
 mcu/_support/AM13E/tests/hal/serial_pa26_host_test.c mcu/AM13E/hal/motor_power_stage.c src/io.c mcu/_support/AM13E/tests/app/rel17_defaults_host.c \
 mcu/AM13E/serial/serial_frame_rx.c mcu/AM13E/hal/serial_tx_queue.c mcu/AM13E/hal/crsf_deferred_command.c mcu/AM13E/hal/bidir_timing.c mcu/AM13E/serial/bidir_codec.c \
 "$out/dl_timer.c" "$dl/dl_unicomm.c" "$dl/dl_unicommuart.c" \
 -o "$out/serial-pa26-test"
"$out/serial-pa26-test"

done

# The 16x path must reject zero, overflowing IBRD and zero-IBRD baud rates.
for invalid_baud in 0 1 6250001; do
  if cc -DSERIAL_BR="$invalid_baud" -fsyntax-only -no-pie -D_GNU_SOURCE -std=c11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-variable -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections \
 -include mcu/_support/AM13E/tests/app/newlib_host.h "-DAM13E_TEST_TI_DMA_HEADER=\"$dl/dl_dma.h\"" -DAM13E_BOARD_MCU_PART=23019 -DAM13E_BOARD_PACKAGE=1 -DAM13E -DAM13E_REF_IO_PLAN_V1=1 -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' \
  \
 "-DAM13E_TEST_TI_ECAP_HEADER=\"$dl/dl_ecap.h\"" "-DAM13E_TEST_TI_GPIO_HEADER=\"$dl/dl_gpio.h\"" "-DAM13E_TEST_TI_MCPWM_HEADER=\"$dl/dl_mcpwm.h\"" \
 "-DAM13E_TEST_TI_EQEP_HEADER=\"$dl/dl_eqep.h\"" \
 "-DAM13E_TEST_TI_UART_HEADER=\"$dl/dl_unicommuart.h\"" -DAM13E_BOARD_SERIAL_RX_CONFIGURED=1 -DAM13E_BOARD_SERIAL_RX_UC_INDEX=0 -DAM13E_BOARD_SERIAL_RX_PINCM=26 -DAM13E_BOARD_SERIAL_RX_PIN_FUNCTION=8 -DAM13E_BOARD_SERIAL_RX_BUSCLK_HZ=100000000 -DAM13E_BOARD_SERIAL_SBUS_PIN_INVERT=1 -DAM13E_BOARD_SERIAL_TX_CONFIGURED=1 -DAM13E_BOARD_SERIAL_TX_PINCM=26 -DAM13E_BOARD_SERIAL_TX_PIN_FUNCTION=9 -DAM13E_BOARD_SERIAL_TX_TWO_WIRE=0 -DAM13E_BOARD_SBUS2_TIMED_REPLY=1 -DAM13E_BOARD_SBUS2_TX_PIN_INVERT=1 \
 -Isrc -Imcu/AM13E/serial -Imcu/AM13E -Imcu/AM13E -Imcu/AM13E/hal -Imcu/_support/AM13E/tests/hal/register_mock -I"$hw" -I"$dl" \
 mcu/_support/AM13E/tests/hal/serial_pa26_host_test.c mcu/AM13E/hal/motor_power_stage.c src/io.c mcu/_support/AM13E/tests/app/rel17_defaults_host.c \
 mcu/AM13E/serial/serial_frame_rx.c mcu/AM13E/hal/serial_tx_queue.c mcu/AM13E/hal/crsf_deferred_command.c mcu/AM13E/hal/bidir_timing.c mcu/AM13E/serial/bidir_codec.c \
 "$out/dl_timer.c" "$dl/dl_unicomm.c" "$dl/dl_unicommuart.c" \
  >"$out/baud-$invalid_baud.log" 2>&1; then
    echo "FAIL: unsupported SERIAL_BR accepted: $invalid_baud" >&2; exit 1
  fi
  grep -q 'SERIAL_BR requires a representable' "$out/baud-$invalid_baud.log"
  echo "PASS: rejected unsupported SERIAL_BR=$invalid_baud"
done

# Actual Boot PA26 software-serial transport; SysTick/peer are host boundaries.
cc -no-pie -D_GNU_SOURCE -std=c11 -O1 -Wall -Wextra -Werror -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
 -DAM13E -DIO_PA26 "-DAM13E_TEST_TI_GPIO_HEADER=\"$dl/dl_gpio.h\"" \
 -Iboot/src -Iboot/mcu/AM13E -Iboot/mcu/AM13E/hal -Imcu/_support/AM13E/tests/boot/pa26_mock \
 -Imcu/_support/AM13E/tests/hal/register_mock -I"$hw" -I"$dl" \
 mcu/_support/AM13E/tests/boot/am13e_pa26_io_host_test.c -o "$out/boot-pa26-test"
"$out/boot-pa26-test"

# Compile the real optional input owner: invalid routing must fail before
# any GPIO mode can overwrite a motor/receiver/sensor pad.
hall_args=(-std=c11 -Werror -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast
 -DAM13E -DCLK=200000000 -DHALL_MAP=1 -DAM13E_REF_IO_PLAN_V1=1
 -DAM13E_HALL_GPIO_TIMESTAMP=1 -DAM13E_HALL_PIN1=12 -DAM13E_HALL_PIN2=28
 -DAM13E_HALL_PIN3=31 -DAM13E_HALL_PULL=DL_GPIO_RESISTOR_PULL_UP
 -DAM13E_HALL_INVERT_MASK=0
 "-DAM13E_TEST_TI_GPIO_HEADER=\"$dl/dl_gpio.h\""
 "-DAM13E_TEST_TI_EQEP_HEADER=\"$dl/dl_eqep.h\""
 -Isrc -Imcu/AM13E -Imcu/AM13E/hal -Imcu/_support/AM13E/tests/hal/register_mock
 -I"$hw" -I"$dl" -fsyntax-only mcu/AM13E/hal/motor_hall.c)
cc "${hall_args[@]}"
for invalid in missing_route motor_pad duplicate_pad bad_inversion unspecified_timestamp; do
  expected='Hall routes conflict'
  case "$invalid" in
    missing_route) args=(-UAM13E_HALL_PIN3); expected='Hall requires three explicit' ;;
    motor_pad) args=(-UAM13E_HALL_PIN1 -DAM13E_HALL_PIN1=8) ;;
    duplicate_pad) args=(-UAM13E_HALL_PIN1 -DAM13E_HALL_PIN1=7) ;;
    bad_inversion) args=(-UAM13E_HALL_INVERT_MASK -DAM13E_HALL_INVERT_MASK=8); expected='Invalid Hall electrical definition' ;;
    unspecified_timestamp) args=(-UAM13E_HALL_GPIO_TIMESTAMP); expected='Hall requires three explicit' ;;
  esac
  if cc "${hall_args[@]}" "${args[@]}" >"$out/hall-$invalid.log" 2>&1; then
    echo "FAIL: invalid Hall definition accepted: $invalid" >&2; exit 1
  fi
  grep -q -- "$expected" "$out/hall-$invalid.log"
  echo "PASS: rejected Hall definition: $invalid"
done

base=(-DAM13E_MOTOR_GATE_ACTIVE_LEVEL=1 -DAM13E_MOTOR_PAD_INVERT_MASK=0
 -DAM13E_MOTOR_OUTPUT_RED_TICKS=120 -DAM13E_MOTOR_OUTPUT_FED_TICKS=0
 -DAM13E_MOTOR_MIN_DEAD_TICKS=100 -DAM13E_MOTOR_NFAULT_ACTIVE_LOW=1
 -DAM13E_MOTOR_GATE_RESET_INACTIVE=1)
header=mcu/AM13E/hal/motor_output_configuration.h
cc -std=c11 -Werror "${base[@]}" -include "$header" -x c -fsyntax-only /dev/null
for invalid in missing_gate bad_gate bad_pad missing_min short_dead nonzero_fed reset_active pwm_enable inverted_mismatch pwm_enable_bad_fed topology_without_selector pwm_enable_missing_driver_dt pwm_enable_short_driver_dt; do
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
    pwm_enable_bad_fed) args=(-DPWM_ENABLE -DAM13E_MOTOR_PWM_ENABLE_TOPOLOGY=1 -DAM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS=1500) ;;
    topology_without_selector) args=(-DAM13E_MOTOR_PWM_ENABLE_TOPOLOGY=1) ;;
    pwm_enable_missing_driver_dt) args=(-DPWM_ENABLE -DAM13E_MOTOR_PWM_ENABLE_TOPOLOGY=1 -UAM13E_MOTOR_OUTPUT_FED_TICKS -DAM13E_MOTOR_OUTPUT_FED_TICKS=120) ;;
    pwm_enable_short_driver_dt) args=(-DPWM_ENABLE -DAM13E_MOTOR_PWM_ENABLE_TOPOLOGY=1 -DAM13E_MOTOR_PWM_ENABLE_DRIVER_DEAD_NS=500 -UAM13E_MOTOR_OUTPUT_FED_TICKS -DAM13E_MOTOR_OUTPUT_FED_TICKS=120) ;;
  esac
  if cc -std=c11 -Werror "${base[@]}" "${args[@]}" -include "$header" -x c -fsyntax-only /dev/null >"$out/$invalid.log" 2>&1; then
    echo "FAIL: invalid motor electrical definition accepted: $invalid" >&2; exit 1
  fi
  echo "PASS: rejected motor electrical definition: $invalid"
done
