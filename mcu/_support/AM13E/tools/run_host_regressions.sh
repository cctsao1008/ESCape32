#!/usr/bin/env bash
# Shared native production/portable regressions. ARM links and hardware qualification
# are separate gates; full here means all native groups, not all milestone gates.
set -euo pipefail
mode=${1:-full}
case "$mode" in fast|full) ;; *) echo 'Usage: run_host_regressions.sh [fast|full]' >&2; exit 2 ;; esac
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$repo"
cc --version | head -n 1
printf 'Native regression mode: %s (no hardware operations)\n' "$mode"
flags="-D_GNU_SOURCE -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -Isrc -Isrc/serial -Isrc/runtime -Imcu/AM13E -Imcu/AM13E/hal -Isrc/tests/AM13E -Isrc/tests/AM13E/interfaces"
run_test () {
  name="$1"
  shift
  if [ "$mode" = fast ]; then
    case "$name" in
      flash_plan|flash_writer|wrp_nonmain|service_handoff|receiver_config_default|led_source|motor_source|config_command_source|servo_source|motor_ownership|fault_trip_route|bidir_codec) ;;
      *) return 0 ;;
    esac
  fi
  case "$name" in
    exbus_autobaud|cli_entry|ibus_receiver|sbus_receiver|sbus2_reply|crsf_receiver|exbus_receiver|exbus_telemetry_reply|hott_receiver|crsf_command_reply|serial_command)
      set -- -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock "$@" src/util.c src/tests/AM13E/rel17_defaults_host.c src/tests/AM13E/core_io_fixture.c ;;
  esac
  if [ "$name" = bemf_commutation ]; then
    set -- -DREL17_BEMF_POLICY_FIXTURE -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock "$@" src/tests/AM13E/rel17_defaults_host.c
  fi
  cc $flags "$@" -o "/tmp/am13e_$name" || exit 1
  "/tmp/am13e_$name" || exit 1
  echo "PASS: $name"
}
run_test flash_plan mcu/AM13E/hal/cfg_flash_plan.c mcu/AM13E/hal/tests/cfg_flash_plan_host_test.c
run_test flash_writer mcu/AM13E/hal/cfg_flash_plan.c mcu/AM13E/hal/cfg_flash_writer.c mcu/AM13E/hal/tests/cfg_flash_writer_host_test.c
run_test wrp_nonmain -Iboot/mcu/AM13E -Iboot/mcu/AM13E/hal boot/mcu/AM13E/hal/wrp_nonmain.c boot/tests/am13e_wrp_nonmain_test.c
run_test adc_scaling mcu/AM13E/hal/analog_calibration_plan.c mcu/AM13E/hal/tests/analog_calibration_plan_host_test.c
run_test ibus_receiver src/tests/AM13E/ibus_receiver_host_test.c
run_test sbus_receiver src/tests/AM13E/sbus_receiver_host_test.c
run_test sbus2_reply src/tests/AM13E/sbus2_reply_host_test.c
run_test crsf_receiver src/tests/AM13E/crsf_receiver_host_test.c
run_test exbus_receiver src/tests/AM13E/exbus_receiver_host_test.c
run_test exbus_autobaud src/tests/AM13E/exbus_autobaud_host_test.c
run_test exbus_telemetry_reply src/tests/AM13E/exbus_telemetry_reply_host_test.c
run_test hott_receiver src/tests/AM13E/hott_receiver_host_test.c
run_test crsf_command_reply src/tests/AM13E/crsf_command_reply_host_test.c
run_test crsf_deferred_command mcu/AM13E/hal/crsf_deferred_command.c mcu/AM13E/hal/tests/crsf_deferred_command_host_test.c
run_test serial_tx_queue mcu/AM13E/hal/serial_tx_queue.c mcu/AM13E/hal/tests/serial_tx_queue_host_test.c
run_test serial_frames src/serial/serial_frame_rx.c src/tests/AM13E/serial_frame_rx_host_test.c
run_test serial_command src/tests/AM13E/serial_command_host_test.c
run_test cli_line_rx src/serial/cli_line_rx.c src/tests/AM13E/cli_line_rx_host_test.c
run_test cli_entry src/tests/AM13E/cli_entry_host_test.c
run_test park_source -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -DPARK_PIN=1 -DERPM_PIN=1 -DAM13E_BOARD_PARK_GPIO=GPIO1 -DAM13E_BOARD_PARK_MASK=1U -DAM13E_BOARD_PARK_PINCM=32U -DAM13E_BOARD_ERPM_GPIO=GPIO1 -DAM13E_BOARD_ERPM_MASK=2U -DAM13E_BOARD_ERPM_PINCM=33U -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock mcu/AM13E/hal/aux_gpio.c mcu/AM13E/hal/motor_phase_plan.c src/tests/AM13E/park_source_host_test.c
run_test aux_wrong_port -DAM13E -DERPM_PIN=1 -DAM13E_BOARD_ERPM_GPIO=GPIO0 -DAM13E_BOARD_ERPM_MASK=2U -DAM13E_BOARD_ERPM_PINCM=33U -Isrc/tests/AM13E/aux_mock mcu/AM13E/hal/aux_gpio.c mcu/AM13E/hal/tests/aux_route_negative_host_test.c
run_test aux_serial_conflict -DAM13E -DERPM_PIN=1 -DAM13E_BOARD_ERPM_GPIO=GPIO1 -DAM13E_BOARD_ERPM_MASK=2U -DAM13E_BOARD_ERPM_PINCM=33U -DAM13E_BOARD_SERIAL_RX_CONFIGURED=1 -DAM13E_BOARD_SERIAL_RX_PINCM=33U -Isrc/tests/AM13E/aux_mock mcu/AM13E/hal/aux_gpio.c mcu/AM13E/hal/tests/aux_route_negative_host_test.c
run_test service_handoff -DAM13E -Iboot/tests/service_mock -Iboot/mcu/AM13E -Iboot/mcu/AM13E/hal mcu/AM13E/hal/service_reset.c boot/mcu/AM13E/hal/handoff.c boot/tests/service_handoff_host_test.c
run_test receiver_config_default -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock  src/io.c src/util.c src/tests/AM13E/rel17_defaults_host.c src/tests/AM13E/receiver_config_source_host_test.c
run_test receiver_config_serial -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock -DAM13E_BOARD_SERIAL_RX_CONFIGURED=1 src/io.c src/util.c src/tests/AM13E/rel17_defaults_host.c src/tests/AM13E/receiver_config_source_host_test.c
run_test receiver_config_disabled -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock -DAM13E_BOARD_SERIAL_RX_CONFIGURED=1 -DDISABLE_EXBUS -DDISABLE_HOTT src/io.c src/util.c src/tests/AM13E/rel17_defaults_host.c src/tests/AM13E/receiver_config_source_host_test.c
run_test led_source -DAM13E_TEST_GPIO_ENABLE_OBSERVER -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -DLED_STAT -DLED2_INV -DLED_MAP=0xAFB5B3 -DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U -DAM13E_BOARD_LED2_GPIO=GPIO1 -DAM13E_BOARD_LED2_MASK=2U -DAM13E_BOARD_LED2_PINCM=33U -DAM13E_BOARD_LED3_GPIO=GPIO1 -DAM13E_BOARD_LED3_MASK=4U -DAM13E_BOARD_LED3_PINCM=34U -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock src/io.c mcu/AM13E/hal/aux_gpio.c src/tests/AM13E/led_source_host_test.c
run_test led_inverted_source -DLED_OD -DAM13E_TEST_GPIO_ENABLE_OBSERVER -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -DLED_STAT -DLED2_INV -DLED_MAP=0xAFB5B3 -DLED_INV -DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U -DAM13E_BOARD_LED2_GPIO=GPIO1 -DAM13E_BOARD_LED2_MASK=2U -DAM13E_BOARD_LED2_PINCM=33U -DAM13E_BOARD_LED3_GPIO=GPIO1 -DAM13E_BOARD_LED3_MASK=4U -DAM13E_BOARD_LED3_PINCM=34U -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock src/io.c mcu/AM13E/hal/aux_gpio.c src/tests/AM13E/led_source_host_test.c
run_test led_four_source -DLED4_OD -DAM13E_TEST_GPIO_ENABLE_OBSERVER -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -DLED_STAT -DLED2_INV -DLED_MAP=0xffffffff -DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U -DAM13E_BOARD_LED2_GPIO=GPIO1 -DAM13E_BOARD_LED2_MASK=2U -DAM13E_BOARD_LED2_PINCM=33U -DAM13E_BOARD_LED3_GPIO=GPIO1 -DAM13E_BOARD_LED3_MASK=4U -DAM13E_BOARD_LED3_PINCM=34U -DAM13E_BOARD_LED4_GPIO=GPIO1 -DAM13E_BOARD_LED4_MASK=8U -DAM13E_BOARD_LED4_PINCM=35U -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock src/io.c mcu/AM13E/hal/aux_gpio.c src/tests/AM13E/led_source_host_test.c
run_test led_wrong_port -DAM13E_BOARD_LED1_GPIO=GPIO0 -DAM13E -DLED_MAP=0xAF -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U -Isrc -Isrc/tests/AM13E/aux_mock mcu/AM13E/hal/aux_gpio.c mcu/AM13E/hal/tests/led_route_negative_host_test.c
run_test led_serial_conflict -DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_SERIAL_RX_CONFIGURED=1 -DAM13E_BOARD_SERIAL_RX_PINCM=32U -DAM13E -DLED_MAP=0xAF -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U -Isrc -Isrc/tests/AM13E/aux_mock mcu/AM13E/hal/aux_gpio.c mcu/AM13E/hal/tests/led_route_negative_host_test.c
run_test aux_led_conflict -DAM13E -DLED_MAP=0xAF -DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U -DERPM_PIN=1 -DAM13E_BOARD_ERPM_GPIO=GPIO1 -DAM13E_BOARD_ERPM_MASK=1U -DAM13E_BOARD_ERPM_PINCM=32U -Isrc -Isrc/tests/AM13E/aux_mock mcu/AM13E/hal/aux_gpio.c mcu/AM13E/hal/tests/aux_route_negative_host_test.c
run_test led_erpm_conflict -DAM13E_BOARD_LED1_GPIO=GPIO1 -DERPM_PIN=1 -DAM13E_BOARD_ERPM_GPIO=GPIO1 -DAM13E_BOARD_ERPM_MASK=1U -DAM13E_BOARD_ERPM_PINCM=32U -DAM13E -DLED_MAP=0xAF -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U -Isrc -Isrc/tests/AM13E/aux_mock mcu/AM13E/hal/aux_gpio.c mcu/AM13E/hal/tests/led_route_negative_host_test.c
run_test motor_source -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/serial -Isrc/runtime -Imcu/AM13E/hal/tests/motor_mock src/util.c mcu/AM13E/hal/motor_bemf_event_plan.c mcu/AM13E/hal/motor_phase_plan.c src/tests/AM13E/motor_source_host_test.c
run_test config_command_source -DBEC_MAP=0xB35 -DAM13E_BOARD_BEC_BITS=2 -DAM13E_BOARD_BEC1_GPIO=GPIO1 -DAM13E_BOARD_BEC1_MASK=1U -DAM13E_BOARD_BEC1_PINCM=32U -DAM13E_BOARD_BEC2_GPIO=GPIO1 -DAM13E_BOARD_BEC2_MASK=2U -DAM13E_BOARD_BEC2_PINCM=33U -DAM13E_TEST_GPIO_ENABLE_OBSERVER -D_GNU_SOURCE -DAM13E -DAM13E_BOARD_SENSORS_CONFIGURED=1 -DCLK=200000000 '-DTARGET_NAME="Host"' -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Isrc/tests/AM13E/aux_mock -include src/tests/AM13E/newlib_host.h src/io.c src/prog.c mcu/AM13E/hal/cfg_flash_plan.c mcu/AM13E/hal/cfg_flash_writer.c src/tests/AM13E/rel17_defaults_host.c src/tests/AM13E/config_command_source_host_test.c
run_test servo_source -DAM13E -DCLK=200000000 -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Iboot/tests/service_mock src/tests/AM13E/core_calibration_fixture.c src/util.c mcu/AM13E/hal/command_decode.c src/tests/AM13E/servo_source_host_test.c
run_test servo_trim_source -DAM13E -DAM13E_BOARD_SERVO_CLOCK_TRIM_CONFIGURED=1 -DCLK=200000000 -funsigned-char -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -Iboot/tests/service_mock src/io.c src/util.c src/tests/AM13E/servo_trim_source_host_test.c
run_test bemf_clock mcu/AM13E/hal/tests/motor_bemf_clock_host_test.c
run_test bemf_commutation -DAM13E mcu/AM13E/hal/motor_bemf_event_plan.c mcu/AM13E/hal/motor_phase_plan.c mcu/AM13E/hal/motor_aq_plan.c mcu/AM13E/hal/motor_timer_math.c mcu/AM13E/hal/tests/motor_bemf_commutation_host_test.c
run_test motor_aq mcu/AM13E/hal/motor_phase_plan.c mcu/AM13E/hal/motor_aq_plan.c mcu/AM13E/hal/tests/motor_aq_plan_host_test.c
run_test motor_brake mcu/AM13E/hal/motor_phase_plan.c mcu/AM13E/hal/motor_aq_plan.c mcu/AM13E/hal/tests/motor_brake_aq_host_test.c
run_test motor_lock mcu/AM13E/hal/motor_phase_plan.c mcu/AM13E/hal/motor_aq_plan.c mcu/AM13E/hal/tests/motor_lock_aq_host_test.c
run_test motor_audio mcu/AM13E/hal/motor_audio_plan.c mcu/AM13E/hal/tests/motor_audio_plan_host_test.c
run_test motor_ownership mcu/AM13E/hal/motor_ownership_plan.c mcu/AM13E/hal/motor_phase_plan.c mcu/AM13E/hal/motor_aq_plan.c mcu/AM13E/hal/motor_audio_plan.c mcu/AM13E/hal/tests/motor_ownership_integration_host_test.c
run_test fault_trip_route mcu/AM13E/hal/fault_trip_route_plan.c mcu/AM13E/hal/tests/fault_trip_route_plan_host_test.c
run_test motor_pad_route mcu/AM13E/hal/motor_output_route_plan.c mcu/AM13E/hal/tests/motor_output_route_plan_host_test.c
run_test board_configuration -DAM13E mcu/AM13E/hal/tests/board_configuration_host_test.c
run_test board_configuration_motor -DAM13E -DAM13E_BOARD_DEVELOPMENT_ACTIVE_TEST -DAM13E_MOTOR_BOARD_DEADBAND_CONFIGURED=1 -DAM13E_BOARD_SENSORS_CONFIGURED=1 mcu/AM13E/hal/tests/board_configuration_host_test.c
cc $flags -DAM13E -DAM13E_BOARD_DEFAULTS_OVERRIDE_TEST \
  -DAM13E_BOARD_PB13_ACTIVE_LEVEL=0 \
  -DAM13E_MOTOR_DB_RED_TICKS=64 \
  -DAM13E_BOARD_ADC_VREF_MV=3000 \
  -DAM13E_BOARD_OC_GPIO_PINCM=49 \
  -DAM13E_BOARD_SERIAL_TX_PINCM=50 \
  -DAM13E_BOARD_CURRENT_SENSE_PINCM=51 \
  mcu/AM13E/hal/tests/board_configuration_host_test.c -o /tmp/am13e_board_configuration_override
/tmp/am13e_board_configuration_override
echo "PASS: board_configuration_override"
run_test bidir_codec src/serial/bidir_codec.c src/tests/AM13E/bidir_codec_host_test.c
run_test bidir_timing src/serial/bidir_codec.c mcu/AM13E/hal/bidir_timing.c mcu/AM13E/hal/tests/bidir_timing_host_test.c
run_test command_bidir_roundtrip mcu/AM13E/hal/command_decode.c src/serial/bidir_codec.c mcu/AM13E/hal/bidir_timing.c src/tests/AM13E/command_bidir_roundtrip_host_test.c
run_test motor_duty mcu/AM13E/hal/motor_duty_plan.c mcu/AM13E/hal/tests/motor_duty_plan_host_test.c
run_test motor_frequency mcu/AM13E/hal/motor_frequency_plan.c mcu/AM13E/hal/tests/motor_frequency_plan_host_test.c
run_test motor_phase mcu/AM13E/hal/motor_phase_plan.c mcu/AM13E/hal/tests/motor_phase_plan_host_test.c
run_test motor_pwm_shadow mcu/AM13E/hal/motor_pwm_shadow_plan.c mcu/AM13E/hal/motor_frequency_plan.c mcu/AM13E/hal/motor_duty_plan.c mcu/AM13E/hal/motor_shadow_plan.c mcu/AM13E/hal/tests/motor_pwm_shadow_plan_host_test.c
run_test motor_shadow mcu/AM13E/hal/motor_shadow_plan.c mcu/AM13E/hal/tests/motor_shadow_plan_host_test.c
run_test motor_timer_math mcu/AM13E/hal/motor_timer_math.c mcu/AM13E/hal/tests/motor_timer_math_host_test.c
run_test command_decode mcu/AM13E/hal/command_decode.c mcu/AM13E/hal/tests/command_decode_host_test.c
run_test telem_modes mcu/AM13E/hal/telem_mode_plan.c mcu/AM13E/hal/tests/telem_mode_plan_host_test.c
run_test telem_frame_rx src/serial/telem_frame_rx.c src/tests/AM13E/telem_frame_rx_host_test.c
run_test telem_tx_progress mcu/AM13E/hal/telem_tx_progress.c mcu/AM13E/hal/tests/telem_tx_progress_host_test.c
run_test arming_window -DAM13E mcu/AM13E/hal/arming_window.c mcu/AM13E/hal/tests/test_arming_window_host.c
run_test sine_carrier mcu/AM13E/preset.c src/tests/AM13E/test_sine_carrier_host.c
cc $flags -DAM13E mcu/AM13E/hal/tests/test_bemf_timeout_host.c -o /tmp/am13e_bemf_timeout
/tmp/am13e_bemf_timeout
echo "PASS: bemf_timeout"
