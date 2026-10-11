# Board-independent ESCape32 / AM13E algorithm port.
# This is deliberately a separate object-only target: no GPIO pinmux,
# gate enable, analog channel, board clock or Flash partition is selected.
# It is NOT a runnable firmware or a motor-output qualification gate.
add_library(AM13E_PORTABLE_LOGIC OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/analog_calibration_plan.c"
    "${PROJECT_SOURCE_DIR}/src/serial/bidir_codec.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/bidir_timing.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/fault_trip_route_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_phase_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_aq_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_audio_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_duty_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_frequency_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_shadow_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_pwm_shadow_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_timer_math.c")
target_include_directories(AM13E_PORTABLE_LOGIC PRIVATE
    "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/mcu/AM13E" "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal")
target_compile_definitions(AM13E_PORTABLE_LOGIC PRIVATE AM13E)
target_compile_options(AM13E_PORTABLE_LOGIC PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)


# Board-neutral ARM compile gate for real TI XBAR/MCPWM fault routing.
# No pins, output levels, or reference profile are selected here.
add_library(AM13E_MCU_FAULT_TRIP_BACKEND OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/fault_trip_backend.c")
target_include_directories(AM13E_MCU_FAULT_TRIP_BACKEND SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E-rev17.3.elf,INCLUDE_DIRECTORIES>")
target_compile_definitions(AM13E_MCU_FAULT_TRIP_BACKEND PRIVATE
    AM13E __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
    __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
target_compile_options(AM13E_MCU_FAULT_TRIP_BACKEND PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)

# Board-independent Rel17 scheduler/backend compile gate. The clock
# provider is an external REQUIRED board implementation; this target
# does not pull in the reference XTAL/PLL startup source.
add_library(AM13E_MCU_RUNTIME_TICK OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/config.c")
target_include_directories(AM13E_MCU_RUNTIME_TICK PRIVATE
    "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/mcu/AM13E" "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal")
target_include_directories(AM13E_MCU_RUNTIME_TICK SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E-rev17.3.elf,INCLUDE_DIRECTORIES>")
target_compile_definitions(AM13E_MCU_RUNTIME_TICK PRIVATE
    AM13E TARGET_NAME="AM13E" __DEVICE_SHORT__="am13e230x"
    __DEVICE_LONG__="AM13E230x" __CPU_SHORT__="m33"
    __CGT_SHORT__="gcc_arm")
target_compile_options(AM13E_MCU_RUNTIME_TICK PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)

# Compile real TI ADC0/1/2 pair initialization without choosing
# a sensor pin or external calibration; object-only, not executable.
add_library(AM13E_MCU_ADC_PAIR_BACKEND OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/analog_sampling_backend.c")
target_include_directories(AM13E_MCU_ADC_PAIR_BACKEND PRIVATE
    "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/mcu/AM13E" "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal")
target_include_directories(AM13E_MCU_ADC_PAIR_BACKEND SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E-rev17.3.elf,INCLUDE_DIRECTORIES>")
target_compile_definitions(AM13E_MCU_ADC_PAIR_BACKEND PRIVATE
    AM13E __DEVICE_SHORT__="am13e230x"
    __DEVICE_LONG__="AM13E230x" __CPU_SHORT__="m33"
    __CGT_SHORT__="gcc_arm")
target_compile_options(AM13E_MCU_ADC_PAIR_BACKEND PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)

# Generic AM13E MCU digital command input / INPUTXBAR routing.
# Explicit board route required; no output, oscillator or ADC defaults.
add_library(AM13E_MCU_COMMAND_INPUT_ROUTE OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/command_input_route_backend.c")
target_include_directories(AM13E_MCU_COMMAND_INPUT_ROUTE PRIVATE
    "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/mcu/AM13E" "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal")
target_include_directories(AM13E_MCU_COMMAND_INPUT_ROUTE SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E-rev17.3.elf,INCLUDE_DIRECTORIES>")
target_compile_definitions(AM13E_MCU_COMMAND_INPUT_ROUTE PRIVATE
    AM13E __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
    __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
target_compile_options(AM13E_MCU_COMMAND_INPUT_ROUTE PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)

# Real TI board-neutral six-MCPWM-pad GPIO/PINCM backend; no gate enable.
# Source-only object, never a qualified ESC output executable.
add_library(AM13E_MCU_MOTOR_PAD_BACKEND OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_output_route_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal/motor_output_backend.c")
target_include_directories(AM13E_MCU_MOTOR_PAD_BACKEND PRIVATE
    "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/mcu/AM13E" "${PROJECT_SOURCE_DIR}/mcu/AM13E/hal")
target_include_directories(AM13E_MCU_MOTOR_PAD_BACKEND SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E-rev17.3.elf,INCLUDE_DIRECTORIES>")
target_compile_definitions(AM13E_MCU_MOTOR_PAD_BACKEND PRIVATE
    AM13E __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
    __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
target_compile_options(AM13E_MCU_MOTOR_PAD_BACKEND PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)
