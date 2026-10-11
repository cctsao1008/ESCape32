# Native Rel17 v1.4 Application source selection and selected Reference board options.
        set(am13e_sources
            "${PROJECT_SOURCE_DIR}/src/main.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/entry.c"
            "${PROJECT_SOURCE_DIR}/src/io.c"
            "${PROJECT_SOURCE_DIR}/src/telem.c" # Original Rel17 telemetry source stays linked
            "${PROJECT_SOURCE_DIR}/src/util.c"
            "${PROJECT_SOURCE_DIR}/src/prog.c"
            # Reference UART adapter not yet linked; conditional Rel17
            # serial input/telemetry remains an implementation gap.
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/irq_vectors.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/reset_cause.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/fault_input.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/analog_sampling_backend.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/analog_runtime.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/analog_calibration_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/command_input_route_backend.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/command_capture.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/command_decode.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/ibus_receiver.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/sbus_receiver.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/sbus2_reply.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/crsf_receiver.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/exbus_receiver.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/exbus_autobaud.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/exbus_telemetry_reply.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/hott_receiver.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/crsf_command_reply.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/crsf_deferred_command.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/serial_tx_queue.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/serial_frame_rx.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/serial_command.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/cli_line_rx.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/telem_frame_rx.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/telem_tx_progress.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/telem_mode_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/telem_backend.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/serial_uart_rx.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/bidir_codec.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/bidir_timing.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/command_reply.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/config.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_runtime_irq.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/arming_window.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/clock_source_reference.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/board_clock_reference.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_output_route_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_output_backend.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/board_motor_output_reference.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_safety.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_power_stage.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_audio.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_audio_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_ownership_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/fault_trip_route_plan.c"
            # Unqualified Reference physical fault route not linked;
            # conditional safety support remains a porting gap.
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_bemf.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_bemf_event_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_sine_table.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_phase_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_aq_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_timer_math.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_frequency_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_duty_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_shadow_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_pwm_shadow_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/cfg_flash_plan.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/cfg_flash_writer.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/cfg_flash_runtime.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/input_watchdog.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_event_timer.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/newlib_syscalls.c"
            # Genuine TI DriverLib objects: delayCycles (common) and
            # Flash read wait-state RAMFUNC (FRI); no replacement stubs.
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_common.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_fri.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_flash.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_flashctl.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_adc.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_ecap.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_cmpss_lite.c"
            "${am13e_sdk_compat_dir}/dl_timer.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_dma.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_unicomm.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_unicommuart.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_mcpwm.c")
        # TI 26.01 dl_unicomm.h defines six legacy UC*_INST_PTR
        # objects inside SDK translation units. Suppress that
        # third-party warning ONLY on these pinned SDK .c files;
        # our native sources retain full -Werror.
        set_source_files_properties(
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_unicomm.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_unicommuart.c"
            PROPERTIES COMPILE_OPTIONS "-Wno-unused-variable")
        set(am13e_source_dir "${PROJECT_SOURCE_DIR}/src")
function(am13e_configure_reference_board target)
        # Reference selects PB14 and has no qualified UART transport.
        # Source-defined serial modes are not permanently excluded.
        target_compile_definitions(${target} PRIVATE
            AM13E_PB14_ONLY AM13E_REF_IO_PLAN_V1=1)
        # Reference software control compiles; external Gate/OC/UART/
        # current routes remain unqualified and physically inactive.
        option(AM13E_MOTOR_RUNTIME_ENABLED
            "Compile Reference motor/ADC software (power stage unqualified)" ON)
        if(AM13E_MOTOR_RUNTIME_ENABLED)
            target_compile_definitions(${target} PRIVATE
                AM13E_MOTOR_BOARD_DEADBAND_CONFIGURED=1
                AM13E_BOARD_SENSORS_CONFIGURED=1)
        endif()

        # Production Board Electrical Configuration is supplied as an
        # externally owned CMake file with an explicit definitions LIST:
        #   set(AM13E_BOARD_BOARD_DEFINITIONS
        #       AM13E_BOARD_PB13_ACTIVE_LEVEL=<board value>
        #       ...RED/FED/OC pin/phase inversion/sensing...)
        # Reference numeric values are not qualified board settings.
        set(AM13E_BOARD_BOARD_PROFILE_FILE "" CACHE FILEPATH
            "Reviewed AM13E reference gate driver/dead-band/independent OC definitions")
        if(AM13E_BOARD_BOARD_PROFILE_FILE)
            if(NOT EXISTS "${AM13E_BOARD_BOARD_PROFILE_FILE}")
                message(FATAL_ERROR "Board profile file not found")
            endif()
            include("${AM13E_BOARD_BOARD_PROFILE_FILE}")
            if(NOT DEFINED AM13E_BOARD_BOARD_DEFINITIONS OR
               NOT AM13E_BOARD_BOARD_DEFINITIONS)
                message(FATAL_ERROR
                    "Board profile must provide AM13E_BOARD_BOARD_DEFINITIONS")
            endif()
            target_compile_definitions(${target} PRIVATE
                ${AM13E_BOARD_BOARD_DEFINITIONS})
        endif()
endfunction()
