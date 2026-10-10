cmake_minimum_required(VERSION 3.15)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_C_FLAGS "-Os -g")
set(CMAKE_C_FLAGS_DEBUG "-Og")
project(ESCape32 C)
if(UNIX AND NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT) # Use 'CMAKE_INSTALL_PREFIX' as alternative system root
	set(CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
	include(GNUInstallDirs)
	include_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})
	link_directories(${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
endif()
if(LIBOPENCM3_DIR)
	include_directories(${LIBOPENCM3_DIR}/include)
	link_directories(${LIBOPENCM3_DIR}/lib)
endif()
add_compile_options(-ffreestanding -ffunction-sections -fdata-sections -fsingle-precision-constant
	-Wall -Wextra -Wpedantic -Wundef -Wshadow -Wredundant-decls -Wstrict-prototypes -Wmissing-prototypes
	-Wno-variadic-macros -Wno-unused-result -Wno-unused-parameter)
add_link_options(-nostartfiles -Wl,--gc-sections)

# TI AM13E is an independent platform. This stage establishes the native
# AM13E compile target; the executable/linker integration follows the C/H port.
# Keep the original legacy add_target() implementation untouched.
function(add_target_ti_am13e name)
    set(AM13E_SDK_ROOT "$ENV{AM13E_SDK_ROOT}" CACHE PATH "TI AM13E SDK root")
    if(NOT AM13E_SDK_ROOT)
        set(AM13E_SDK_ROOT "$ENV{HOME}/ti/am13e230x_sdk_26_01_00_03"
            CACHE PATH "TI AM13E SDK root" FORCE)
    endif()
    if(NOT EXISTS "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_mcpwm.h")
        message(FATAL_ERROR "TI AM13E SDK not found; set AM13E_SDK_ROOT")
    endif()

    # Correct TI DriverLib and GCC startup source in the isolated CMake
    # binary tree, never modifying the installed ~/ti SDK.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(am13e_sdk_compat_dir "${PROJECT_BINARY_DIR}/am13e-sdk-compat")
    set(am13e_sdk_compat_script
        "${PROJECT_SOURCE_DIR}/mcu/AM13E/tools/prepare_sdk_sources.py")
    set_property(DIRECTORY "${PROJECT_SOURCE_DIR}" APPEND PROPERTY
        CMAKE_CONFIGURE_DEPENDS
        "${am13e_sdk_compat_script}"
        "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_timer.c"
        "${AM13E_SDK_ROOT}/source/device/am13e230x/src/startup_gcc_arm.c")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${am13e_sdk_compat_script}"
            --sdk-root "${AM13E_SDK_ROOT}"
            --output "${am13e_sdk_compat_dir}"
        RESULT_VARIABLE am13e_sdk_compat_rc
        OUTPUT_VARIABLE am13e_sdk_compat_stdout
        ERROR_VARIABLE am13e_sdk_compat_stderr)
    if(NOT am13e_sdk_compat_rc EQUAL 0)
        message(FATAL_ERROR "AM13E TI SDK source correction failed:\n"
            "${am13e_sdk_compat_stdout}\n${am13e_sdk_compat_stderr}")
    endif()
    # ESCape32 owns the AM13E build and product configuration.
    # TI SDK provides MCU headers / DriverLib / CMSIS, analogous to
    # libopencm3 on the existing STM32 targets. Do NOT inherit a TI
    # example board's generated SysConfig as a default.
    set(AM13E_PROJECT_SYSCFG_DIR "" CACHE PATH
        "Optional product-owned AM13E generated configuration headers")
    if(AM13E_PROJECT_SYSCFG_DIR AND
       NOT IS_DIRECTORY "${AM13E_PROJECT_SYSCFG_DIR}")
        message(FATAL_ERROR
            "AM13E_PROJECT_SYSCFG_DIR is not an existing directory")
    endif()
    # Delegate MCU source ownership through original add_target(name mcu ...).
    if(name MATCHES "^BOOT")
        include("${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/config.cmake")
    else()
        include("${PROJECT_SOURCE_DIR}/mcu/AM13E/config.cmake")
    endif()
    if(name MATCHES "^BOOT")
        set(AM13E_BOOT_LINKER_SCRIPT "" CACHE FILEPATH
            "AM13E boot linker script selected for the target board")
        if(AM13E_BOOT_LINKER_SCRIPT)
            set(am13e_startup
                "${am13e_sdk_compat_dir}/startup_gcc_arm.c")
            if(NOT EXISTS "${am13e_startup}")
                message(FATAL_ERROR "TI AM13E GCC startup source not found")
            endif()
            list(APPEND am13e_sources "${am13e_startup}")
            if(NOT EXISTS "${AM13E_BOOT_LINKER_SCRIPT}")
                message(FATAL_ERROR "AM13E_BOOT_LINKER_SCRIPT does not exist")
            endif()
            add_executable(${name}.elf ${am13e_sources})
            target_link_options(${name}.elf PRIVATE
                -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${name}.map
                -T${AM13E_BOOT_LINKER_SCRIPT}
                -Wl,--no-undefined -Wl,--fatal-warnings
                -specs=nosys.specs
                -Wl,--require-defined=Reset_Handler
                -Wl,--require-defined=main
                -Wl,--require-defined=initio
                -Wl,--require-defined=recvbuf
                -Wl,--require-defined=sendbuf
                -Wl,--require-defined=crc32
                -Wl,--require-defined=write
                -Wl,--require-defined=boot_am13e_launch_application)
            set(am13e_binary ${name}.elf)
        else()
            # Without a validated board linker contract this stage compiles
            # sources only. Never report it as a linked firmware image.
            add_library(${name} OBJECT ${am13e_sources})
            set(am13e_binary ${name})
        endif()
    else()
        add_library(${name} OBJECT ${am13e_sources})
        set(am13e_binary ${name})
    endif()
    target_compile_definitions(${am13e_binary} PRIVATE
        AM13E TARGET_NAME="${name}" ${ARGN}
        __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
        __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
    if(NOT name MATCHES "^BOOT")
        am13e_configure_reference_board(${am13e_binary})
    endif()
    # Genuine source corrections rather than warning suppression.
    # Scope strict builds to AM13E reference only, never legacy STM32 boards.
    target_compile_options(${am13e_binary} PRIVATE
        -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
        -mfloat-abi=hard -fno-branch-count-reg -Werror)
    if(TARGET ${name}.elf)
        # Match linker multilib selection to the AM13E object ABI.
        target_link_options(${name}.elf PRIVATE
            -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
            -mfloat-abi=hard -fno-branch-count-reg)
    endif()
    target_include_directories(${am13e_binary} PRIVATE "${am13e_source_dir}")
    if(name MATCHES "^BOOT")
        target_include_directories(${am13e_binary} PRIVATE "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E")
    else()
        # Rel17 Application gets its own platform config.h. Never reuse
        # boot/mcu/AM13E/config.h (which is for the PB14 boot service).
        target_include_directories(${am13e_binary} PRIVATE
            "${PROJECT_SOURCE_DIR}/mcu/AM13E")
    endif()
    if(AM13E_PROJECT_SYSCFG_DIR)
        target_include_directories(${am13e_binary} PRIVATE
            "${AM13E_PROJECT_SYSCFG_DIR}")
    endif()
    target_include_directories(${am13e_binary} SYSTEM PRIVATE
        "${AM13E_SDK_ROOT}/ti_sdk_config/am13e230x/default/device_support/include"
        "${AM13E_SDK_ROOT}/source/device/am13e230x/include"
        "${AM13E_SDK_ROOT}/source/device/am13e230x/include/hw"
        "${AM13E_SDK_ROOT}/source/driverlib/am13e230x"
        "${AM13E_SDK_ROOT}/source/arch/include"
        "${AM13E_SDK_ROOT}/source/arch/m33/include"
        "${AM13E_SDK_ROOT}/source/cmsis/Core/Include"
        "${AM13E_SDK_ROOT}/source/compiler/m33_gcc_arm")
endfunction()

function(add_target name mcu)
    if(mcu STREQUAL "AM13E")
        add_target_ti_am13e(${name} ${ARGN})
        return()
    endif()
	include(mcu/${mcu}/config.cmake)
	file(READ src/main.c main_c)
	string(REGEX MATCH "REVISION [0-9]+" rev1 ${main_c})
	string(REGEX MATCH "REVPATCH [0-9]+" rev2 ${main_c})
	string(REGEX REPLACE "[^0-9]+" "-rev" rev1 ${rev1})
	if (rev2)
		string(REGEX REPLACE "[^0-9]+" "." rev2 ${rev2})
	endif()
	set(elf ${name}${rev1}${rev2}.elf)
	set(bin ${name}${rev1}${rev2}.bin)
	set(hex ${name}${rev1}${rev2}.hex)
	file(GLOB srcs src/*.c mcu/${mcu}/*.c)
	add_executable(${elf} ${srcs})
	if("SMALL_DEVICE" IN_LIST ARGN)
		target_compile_options(${elf} PRIVATE -Wno-unused-variable -Wno-unused-function)
		set(size 32K)
	else()
		set(size 64K)
	endif()
	target_include_directories(${elf} PRIVATE src mcu/${mcu})
	target_compile_options(${elf} PRIVATE ${opts})
	target_compile_definitions(${elf} PRIVATE ${mcu} ${name} ${defs} TARGET_NAME="${name}" ${ARGN})
	target_link_options(${elf} PRIVATE -Wl,--defsym,FLASH_SIZE=${size} -T${CMAKE_CURRENT_SOURCE_DIR}/mcu/${mcu}/config.ld -T${CMAKE_CURRENT_SOURCE_DIR}/mcu/common.ld ${opts})
	target_link_libraries(${elf} c_nano gcc nosys ${libs})
	add_custom_command(
		OUTPUT ${bin} ${hex}
		COMMAND arm-none-eabi-objcopy -O binary ${elf} ${bin}
		COMMAND arm-none-eabi-objcopy -O ihex ${elf} ${hex}
		DEPENDS ${elf}
	)
	add_custom_target(${name} ALL DEPENDS ${hex})
	add_custom_target(flash-${name} COMMAND st-flash --reset --connect-under-reset --format ihex write ${hex} DEPENDS ${hex})
endfunction()

# Stage C2: opt-in ARM Rel17 v1.4 vector-first linker/packer smoke fixture.
# Not production FW1/FW2; the full Rel17 FW1 strict linked image is separate.
# Legacy packer filename below implements the current flat v1.4 format.
option(AM13E_ENABLE_APP_SMOKE
    "Enable AM13E Rel17 v1.4 ARM app-link / flat packer smoke validation" OFF)
if(AM13E_ENABLE_APP_SMOKE)
    set(am13e_smoke_script
        "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/linker_app_smoke.ld")
    set(am13e_smoke_source
        "${PROJECT_SOURCE_DIR}/boot/tests/am13e_app_link_smoke.c")
    set(am13e_smoke_packer
        "${PROJECT_SOURCE_DIR}/boot/tools/pack_am13e_v2.py")
    set(am13e_smoke_verify
        "${PROJECT_SOURCE_DIR}/boot/tests/am13e_app_link_smoke_test.py")
    find_program(AM13E_OBJCOPY arm-none-eabi-objcopy)
    find_program(AM13E_OBJDUMP arm-none-eabi-objdump)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    if(NOT AM13E_OBJCOPY OR NOT AM13E_OBJDUMP)
        message(FATAL_ERROR
            "Stage C2 requires arm-none-eabi-objcopy and arm-none-eabi-objdump")
    endif()

    add_executable(AM13E_APP_SMOKE.elf "${am13e_smoke_source}")
    target_compile_options(AM13E_APP_SMOKE.elf PRIVATE
        -march=armv8.1-m.main -mthumb
        -mfpu=fpv5-sp-d16 -mfloat-abi=hard)
    target_link_options(AM13E_APP_SMOKE.elf PRIVATE
        -march=armv8.1-m.main -mthumb
        -mfpu=fpv5-sp-d16 -mfloat-abi=hard
        -nostdlib -Wl,--no-undefined
        "-T${am13e_smoke_script}"
        "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/AM13E_APP_SMOKE.map")
    set(am13e_smoke_raw
        "${CMAKE_CURRENT_BINARY_DIR}/AM13E_APP_SMOKE.bin")
    set(am13e_smoke_packed
        "${CMAKE_CURRENT_BINARY_DIR}/AM13E_APP_SMOKE.flat-smoke.bin")
    set(am13e_smoke_manifest
        "${CMAKE_CURRENT_BINARY_DIR}/AM13E_APP_SMOKE.flat-smoke.json")

    add_custom_command(
        OUTPUT "${am13e_smoke_raw}"
        # The zero-size .data section is ALLOC/LOAD with VMA/LMA
        # 0x20000000 on GNU ld. objcopy --gap-fill would otherwise
        # expand the 0x6000-based flat BIN to ~512 MiB. This opt-in
        # smoke fixture asserts SIZEOF(.data)==0 in its linker script,
        # so excluding it is safe. Production APP must retain a
        # proper initialized-data load image and is NOT covered here.
        COMMAND "${AM13E_OBJCOPY}" -O binary --gap-fill=0xff
            --remove-section=.data
            "$<TARGET_FILE:AM13E_APP_SMOKE.elf>" "${am13e_smoke_raw}"
        DEPENDS AM13E_APP_SMOKE.elf "${am13e_smoke_script}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${am13e_smoke_packed}" "${am13e_smoke_manifest}"
        COMMAND "${Python3_EXECUTABLE}" "${am13e_smoke_packer}" pack
            "${am13e_smoke_raw}" "${am13e_smoke_packed}"
            --manifest "${am13e_smoke_manifest}"
        COMMAND "${Python3_EXECUTABLE}" "${am13e_smoke_packer}" verify
            "${am13e_smoke_packed}"
        COMMAND "${Python3_EXECUTABLE}" "${am13e_smoke_verify}"
            "${am13e_smoke_raw}" "${am13e_smoke_packed}"
            "${am13e_smoke_manifest}" "${AM13E_OBJDUMP}"
            "$<TARGET_FILE:AM13E_APP_SMOKE.elf>"
        DEPENDS "${am13e_smoke_raw}"
            "${am13e_smoke_packer}" "${am13e_smoke_verify}"
        VERBATIM)
    add_custom_target(AM13E_APP_SMOKE DEPENDS
        "${am13e_smoke_packed}" "${am13e_smoke_manifest}")
endif()

add_subdirectory(boot)

add_target(AART1 AT32F421 DEAD_TIME=0 COMP_MAP=123 ARM=0 VOLUME=0 INPUT_MODE=1 ANALOG_CHAN=6 ANALOG_MIN=270 ANALOG_MAX=1440 DUTY_MIN=100 DUTY_SPUP=25 FULL_DUTY IO_RXTX)
add_target(AIRBOT1 AT32F421 DEAD_TIME=66 COMP_MAP=321 SENS_MAP=0xA3A6 VOLT_MUL=74 CURR_MUL=30 LED_MAP=0xAFB3B4)
add_target(AIRBOT2 STM32F051 DEAD_TIME=26 COMP_MAP=321 SENS_MAP=0xA3 VOLT_MUL=74 IO_PA2 SMALL_DEVICE DISABLE_HOTT)
add_target(EMAX1 STM32F051 DEAD_TIME=26 COMP_MAP=123 IO_PA2 SMALL_DEVICE)
add_target(ESCAPE1 STM32G071 DEAD_TIME=35 COMP_MAP=123 HALL_MAP=0xAFB35 SENS_MAP=0xA6A5A4 VOLT_MUL=110 CURR_MUL=40 BEC_MAP=0xCEF LED_WS2812 LED_STAT IO_PA2)
add_target(ESCAPE2 STM32G071 DEAD_TIME=35 COMP_MAP=123 SENS_MAP=0xA6 VOLT_MUL=110 BEC_MAP=0xADE IO_PA2)
add_target(ESCAPE3 AT32F421 DEAD_TIME=66 COMP_MAP=123 SENS_MAP=0xA6 VOLT_MUL=110 BEC_MAP=0xADE)
add_target(ESCAPE4 STM32G071 DEAD_TIME=35 COMP_MAP=123 HALL_MAP=0xB358 SENS_MAP=0xA6A5A4 VOLT_MUL=110 CURR_MUL=40 BEC_MAP=0xCEF LED_MAP=0xF2AF LED1_INV LED_STAT IO_PA2)
add_target(ESCAPE5 STM32G071 DEAD_TIME=35 COMP_MAP=123 HALL_MAP=0xAFB35 SENS_MAP=0xA6 VOLT_MUL=110 BEC_MAP=0xA45 LED_WS2812 LED_STAT IO_PA2)
add_target(FLYCOLOR1 STM32F051 DEAD_TIME=26 COMP_MAP=123 SENS_MAP=0xA6 VOLT_MUL=110 LED_MAP=0xB5B4B3 IO_PA2 SMALL_DEVICE DISABLE_HOTT)
add_target(FLYCOLOR2 STM32G071 DEAD_TIME=35 COMP_MAP=123 SENS_MAP=0xA6 VOLT_MUL=110 LED_MAP=0xB8 LED_STAT)
add_target(FLYCOLOR3 STM32G071 DEAD_TIME=50 COMP_MAP=123 SENS_MAP=0xA6A4 VOLT_MUL=210 CURR_MUL=30)
add_target(FLYINGRC1 AT32F421 DEAD_TIME=66 COMP_MAP=123)
add_target(FREELYRC1 AT32F421 DEAD_TIME=66 COMP_MAP=123 SENS_MAP=0xA3 VOLT_MUL=110 LED_WS2812 LED_STAT)
add_target(GEPRC1 AT32F421 DEAD_TIME=66 COMP_MAP=123 SENS_MAP=0xA3 VOLT_MUL=110)
add_target(HAKRC1 STM32F051 DEAD_TIME=26 COMP_MAP=213 SENS_MAP=0xA3 VOLT_MUL=110 LED_MAP=0xAFB5B3 LED_INV LED_OD SMALL_DEVICE)
add_target(HAKRC2 AT32F421 DEAD_TIME=66 COMP_MAP=213 SENS_MAP=0xA3 VOLT_MUL=110 LED_MAP=0xAFB5B3 LED_INV LED_OD)
add_target(HAKRC3 STM32G071 DEAD_TIME=35 COMP_MAP=132 SENS_MAP=0xA1A5 VOLT_MUL=110 CURR_MUL=20 LED_WS2812 LED_STAT)
add_target(HGLRC1 STM32F051 DEAD_TIME=26 COMP_MAP=123 SENS_MAP=0xA6 VOLT_MUL=210 IO_PA2 SMALL_DEVICE DISABLE_HOTT)
add_target(HHOBBIES1 GD32E230 DEAD_TIME=40 COMP_MAP=321 SENS_MAP=0xA6 VOLT_MUL=110 LED_MAP=0xAF LED_INV LED_STAT)
add_target(HOBBYWING1 STM32F051 DEAD_TIME=40 COMP_MAP=123 IO_PA2 SMALL_DEVICE)
add_target(HOBBYWING2 AT32F421 DEAD_TIME=96 COMP_MAP=123)
add_target(HOLYBRO1 GD32F350 DEAD_TIME=57 COMP_MAP=321 SENS_MAP=0xA6A0 VOLT_MUL=129 CURR_MUL=50 USE_COMP2)
add_target(IFLIGHT1 STM32F051 DEAD_TIME=26 COMP_MAP=321 SENS_MAP=0xA3A6 VOLT_MUL=110 CURR_MUL=60 LED_MAP=0xB8B5B3 LED_INV SMALL_DEVICE)
add_target(IFLIGHT2 STM32G071 DEAD_TIME=35 COMP_MAP=213 SENS_MAP=0xA5A4 VOLT_MUL=110 CURR_MUL=20 LED_WS2812 IO_PA6)
add_target(IFLIGHT3 STM32G071 DEAD_TIME=35 COMP_MAP=132 SENS_MAP=0xA0 VOLT_MUL=110 LED_WS2812)
add_target(IFLIGHT4 STM32G071 DEAD_TIME=35 COMP_MAP=213 SENS_MAP=0xA5 VOLT_MUL=110 LED_WS2812 IO_PA6)
add_target(LUMENIER1 GD32F350 DEAD_TIME=57 COMP_MAP=321 SENS_MAP=0xA3 VOLT_MUL=110)
add_target(LYI1 AT32F421 DEAD_TIME=66 COMP_MAP=321 SENS_MAP=0xA3 VOLT_MUL=110)
add_target(NEUTRON1 AT32F421 DEAD_TIME=66 COMP_MAP=321)
add_target(NEUTRON2 AT32F421 DEAD_TIME=66 COMP_MAP=321 INVERTED_HIGH)
add_target(NEUTRON3 STM32L431 DEAD_TIME=44 COMP_MAP=123 SENS_MAP=0xA6A3 VOLT_MUL=210 CURR_MUL=125 USE_COMP2 USE_OPAMP)
add_target(ODDITYRC1 AT32F421 DEAD_TIME=66 COMP_MAP=123 SENS_MAP=0xA3BF VOLT_MUL=110 CURR_MUL=40)
add_target(PHOTONDRIVE1 STM32G431 DEAD_TIME=155 COMP_MAP=132 SENS_MAP=0xBFA6 VOLT_MUL=224 CURR_MUL=63 TEMP_ADC2 TEMP_CHAN=14 TEMP_FUNC=NTC10K3455UP10K LED_WS2812 LED_STAT USE_PB1 USE_HSE=8 DUTY_SPUP=5 DUTY_RATE=20 PROT_TEMP=100)
add_target(RHINO1 STM32F051 DEAD_TIME=26 COMP_MAP=321 SENS_MAP=0xA6 VOLT_MUL=110 SMALL_DEVICE)
add_target(SEQURE1 STM32G071 DEAD_TIME=35 COMP_MAP=123 SENS_MAP=0xA6A4 VOLT_MUL=210 CURR_MUL=30 BEC_MAP=0xCEF BEC_MIN=1 LED_WS2812 LED_STAT)
add_target(SEQURE2 STM32G071 DEAD_TIME=50 COMP_MAP=123 SENS_MAP=0xA6A4 VOLT_MUL=210 CURR_MUL=87 BEC_MAP=0xCEF BEC_MIN=1 LED_WS2812 LED_STAT ERPM_PIN=5)
add_target(SEQURE3 STM32G071 DEAD_TIME=60 COMP_MAP=123 SENS_MAP=0xA6A4 VOLT_MUL=480 CURR_MUL=205 BEC_MAP=0xCEF BEC_MIN=1 LED_WS2812 LED_STAT ERPM_PIN=5)
add_target(SKYSTARS1 GD32E230 DEAD_TIME=40 COMP_MAP=321 SENS_MAP=0xA3 VOLT_MUL=110 LED_MAP=0xB5B3AF)
add_target(TMOTOR1 STM32F051 DEAD_TIME=26 COMP_MAP=132 IO_PA2 SMALL_DEVICE)
add_target(TMOTOR2 STM32F051 DEAD_TIME=26 COMP_MAP=321 SMALL_DEVICE)
add_target(TMOTOR3 STM32G071 DEAD_TIME=35 COMP_MAP=231 IO_PA6)
add_target(TMOTOR4 STM32G071 DEAD_TIME=35 COMP_MAP=123)
add_target(VWING1 STM32G071 DEAD_TIME=35 COMP_MAP=123 SENS_MAP=0xA5A4 VOLT_MUL=110 CURR_MUL=40 BEC_MAP=0xB35 BEC_MIN=1 LED_WS2812 LED_STAT IO_PA2 IO_AUX)

# TI AM13E23019 generic application target.
add_target(AM13E AM13E)

# Board-independent ESCape32 / AM13E algorithm port.
# This is deliberately a separate object-only target: no GPIO pinmux,
# gate enable, analog channel, board clock or Flash partition is selected.
# It is NOT a runnable firmware or a motor-output qualification gate.
add_library(AM13E_PORTABLE_LOGIC OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/analog_calibration_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/bidir_codec.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/bidir_timing.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/fault_trip_route_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_phase_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_aq_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_audio_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_duty_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_frequency_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_shadow_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_pwm_shadow_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_timer_math.c")
target_include_directories(AM13E_PORTABLE_LOGIC PRIVATE
    "${PROJECT_SOURCE_DIR}/mcu/AM13E")
target_compile_definitions(AM13E_PORTABLE_LOGIC PRIVATE AM13E)
target_compile_options(AM13E_PORTABLE_LOGIC PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)


# Board-neutral ARM compile gate for real TI XBAR/MCPWM fault routing.
# No pins, output levels, or reference profile are selected here.
add_library(AM13E_MCU_FAULT_TRIP_BACKEND OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/fault_trip_backend.c")
target_include_directories(AM13E_MCU_FAULT_TRIP_BACKEND SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E,INCLUDE_DIRECTORIES>")
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
    "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/mcu/AM13E")
target_include_directories(AM13E_MCU_RUNTIME_TICK SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E,INCLUDE_DIRECTORIES>")
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
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/analog_sampling_backend.c")
target_include_directories(AM13E_MCU_ADC_PAIR_BACKEND PRIVATE
    "${PROJECT_SOURCE_DIR}/mcu/AM13E")
target_include_directories(AM13E_MCU_ADC_PAIR_BACKEND SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E,INCLUDE_DIRECTORIES>")
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
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/command_input_route_backend.c")
target_include_directories(AM13E_MCU_COMMAND_INPUT_ROUTE PRIVATE
    "${PROJECT_SOURCE_DIR}/mcu/AM13E")
target_include_directories(AM13E_MCU_COMMAND_INPUT_ROUTE SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E,INCLUDE_DIRECTORIES>")
target_compile_definitions(AM13E_MCU_COMMAND_INPUT_ROUTE PRIVATE
    AM13E __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
    __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
target_compile_options(AM13E_MCU_COMMAND_INPUT_ROUTE PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)

# Real TI board-neutral six-MCPWM-pad GPIO/PINCM backend; no gate enable.
# Source-only object, never a qualified ESC output executable.
add_library(AM13E_MCU_MOTOR_PAD_BACKEND OBJECT EXCLUDE_FROM_ALL
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_output_route_plan.c"
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/motor_output_backend.c")
target_include_directories(AM13E_MCU_MOTOR_PAD_BACKEND PRIVATE
    "${PROJECT_SOURCE_DIR}/mcu/AM13E")
target_include_directories(AM13E_MCU_MOTOR_PAD_BACKEND SYSTEM PRIVATE
    "$<TARGET_PROPERTY:AM13E,INCLUDE_DIRECTORIES>")
target_compile_definitions(AM13E_MCU_MOTOR_PAD_BACKEND PRIVATE
    AM13E __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
    __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
target_compile_options(AM13E_MCU_MOTOR_PAD_BACKEND PRIVATE
    -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)

# First real AM13E *diagnostic* ELF: startup + clock only, built from
# the ESCape32 root CMake. This intentionally does not pull in the
# incomplete Rel17 motor-control entry or invent link-only HAL stubs.
# Keep AM13E (all Rel17 sources) as the main FW1 OBJECT target.
option(AM13E_BUILD_CLOCK_DIAGNOSTIC
    "Expose separately named AM13E clock/startup integration ELF"
    ON)
if(AM13E_BUILD_CLOCK_DIAGNOSTIC)
    set(am13e_clock_diag_linker
        "${PROJECT_SOURCE_DIR}/mcu/AM13E/linker_app_reference.ld")
    # Opt-in only: never make legacy/default builds depend on
    # an intentionally pre-production diagnostic ELF.
    add_executable(AM13E_CLOCK.elf EXCLUDE_FROM_ALL
        "${PROJECT_SOURCE_DIR}/mcu/AM13E/clock_diagnostic_entry.c"
        "${PROJECT_SOURCE_DIR}/mcu/AM13E/config.c"
        "${PROJECT_SOURCE_DIR}/mcu/AM13E/clock_source_reference.c"
        "${PROJECT_SOURCE_DIR}/mcu/AM13E/board_clock_reference.c"
        "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_common.c"
        "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_fri.c"
        "${PROJECT_BINARY_DIR}/am13e-sdk-compat/startup_gcc_arm.c")

    # Inherit the SAME TI MCU include/ABI environment as the real
    # ESCape32 AM13E target; do not adopt an SDK example build.
    target_include_directories(AM13E_CLOCK.elf SYSTEM PRIVATE
        "$<TARGET_PROPERTY:AM13E,INCLUDE_DIRECTORIES>")
    target_compile_definitions(AM13E_CLOCK.elf PRIVATE
        AM13E TARGET_NAME="AM13E_CLOCK"
        __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
        __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
    target_compile_options(AM13E_CLOCK.elf PRIVATE
        -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
        -mfloat-abi=hard -fno-branch-count-reg -Werror)
    target_link_options(AM13E_CLOCK.elf PRIVATE
        -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
        -mfloat-abi=hard -fno-branch-count-reg
        "-T${am13e_clock_diag_linker}"
        -Wl,--no-undefined -Wl,--fatal-warnings
        -Wl,--require-defined=Reset_Handler
        -Wl,--require-defined=main
        "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/AM13E_CLOCK.map"
        -specs=nosys.specs)
    # The linker file is the existing historical Boot-format reference
    # and is not a product FW1 linker. This executable has no DShot,
    # safety-qualified outputs or other motor-control functionality.
    set_target_properties(AM13E_CLOCK.elf PROPERTIES
        OUTPUT_NAME "AM13E_CLOCK"
        SUFFIX ".elf")
endif()

# Strict FULL FW1 Rel17 link gate (not expected to pass until hardware
# ports are genuinely implemented). Link all original ESCape32 AM13E
# objects instead of generating an SDK sample firmware or fake HAL.
#
# The current linker is a non-production Boot-format reference.
# This gate must not be flashed or counted as product FW1 acceptance.
option(AM13E_ENABLE_FW1_LINK_GATE
    "Expose strict full-Rel17 AM13E FW1 link target for real backend progress"
    ON)
if(AM13E_ENABLE_FW1_LINK_GATE)
    add_executable(AM13E_FW1.elf EXCLUDE_FROM_ALL
        "${PROJECT_BINARY_DIR}/am13e-sdk-compat/startup_gcc_arm.c"
        $<TARGET_OBJECTS:AM13E>)
    target_include_directories(AM13E_FW1.elf SYSTEM PRIVATE
        "$<TARGET_PROPERTY:AM13E,INCLUDE_DIRECTORIES>")
    target_compile_definitions(AM13E_FW1.elf PRIVATE
        AM13E TARGET_NAME="AM13E"
        __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
        __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
    target_compile_options(AM13E_FW1.elf PRIVATE
        -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
        -mfloat-abi=hard -fno-branch-count-reg -Werror)
    target_link_options(AM13E_FW1.elf PRIVATE
        -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
        -mfloat-abi=hard -fno-branch-count-reg
        "-T${PROJECT_SOURCE_DIR}/mcu/AM13E/linker_app_reference.ld"
        -Wl,--no-undefined -Wl,--fatal-warnings
        -Wl,--require-defined=Reset_Handler
        -Wl,--require-defined=main
        -Wl,--require-defined=SysTick_Handler
        -Wl,--require-defined=PendSV_Handler
        -Wl,--require-defined=HardFault_Handler
        "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/AM13E_FW1.map"
        -specs=nosys.specs)
    set_target_properties(AM13E_FW1.elf PROPERTIES
        OUTPUT_NAME "AM13E_FW1" SUFFIX ".elf")
endif()

# Rev1.4 active single APP + Boot config.ld linkers and flat BIN packer.
# APP_BASE 0x6000; Cfg 0x4000, Reserved 0x5000, APP allocation MAX 488KiB.
# Actual firmware length is determined by the linked ELF-derived BIN.
# Original wire CRC remains; no APP header, whole-image CRC, signature-last.
# NONE remains available only for object-only source-port compilation.
set(AM13E_IMAGE_PROFILE "REL17_V14" CACHE STRING
    "AM13E linked image profile (REL17_V14 or NONE)")
set_property(CACHE AM13E_IMAGE_PROFILE PROPERTY STRINGS REL17_V14 NONE)
option(AM13E_ENABLE_FW1_REL17_IMAGE
    "Enable AM13E reference v1.4 Rel17 application linked ELF and packed image" ON)
if(AM13E_ENABLE_FW1_REL17_IMAGE)
    if(NOT AM13E_IMAGE_PROFILE STREQUAL "REL17_V14")
        message(FATAL_ERROR
            "Unsupported AM13E_IMAGE_PROFILE for linked FW1; "
            "only REL17_V14 ABI exists. Disable FW1_REL17_IMAGE "
            "and select NONE for generic source-only port work.")
    endif()
    include("${PROJECT_SOURCE_DIR}/mcu/AM13E/profiles/image_rel17_v14.cmake")
    if(AM13E_BOOT_LINKER_SCRIPT AND NOT
       AM13E_BOOT_LINKER_SCRIPT STREQUAL AM13E_PROFILE_BOOT_LINKER_SCRIPT)
        message(FATAL_ERROR
            "REL17_V14 image cannot pair with an incompatible Boot linker")
    endif()
    find_program(AM13E_REL17_OBJCOPY arm-none-eabi-objcopy REQUIRED)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_executable(AM13E_FW1_REL17.elf EXCLUDE_FROM_ALL
        "${PROJECT_BINARY_DIR}/am13e-sdk-compat/startup_gcc_arm.c"
        $<TARGET_OBJECTS:AM13E>)
    target_include_directories(AM13E_FW1_REL17.elf SYSTEM PRIVATE
        "$<TARGET_PROPERTY:AM13E,INCLUDE_DIRECTORIES>")
    target_compile_definitions(AM13E_FW1_REL17.elf PRIVATE
        AM13E TARGET_NAME="AM13E"
        __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
        __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
    target_compile_options(AM13E_FW1_REL17.elf PRIVATE
        -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
        -mfloat-abi=hard -fno-branch-count-reg -Werror)
    target_link_options(AM13E_FW1_REL17.elf PRIVATE
        -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
        -mfloat-abi=hard -fno-branch-count-reg
        "-T${AM13E_PROFILE_APP_LINKER_SCRIPT}"
        -Wl,--no-undefined -Wl,--fatal-warnings -specs=nosys.specs
        -Wl,--require-defined=Reset_Handler
        -Wl,--require-defined=main
        -Wl,--require-defined=HardFault_Handler
        -Wl,--require-defined=PendSV_Handler
        -Wl,--require-defined=SysTick_Handler
        "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/AM13E_FW1_REL17.map")
    set_target_properties(AM13E_FW1_REL17.elf PROPERTIES
        OUTPUT_NAME "AM13E_FW1_REL17" SUFFIX ".elf")
    set(am13e_rel17_raw "${CMAKE_CURRENT_BINARY_DIR}/AM13E_FW1_REL17.bin")
    set(am13e_rel17_packed "${CMAKE_CURRENT_BINARY_DIR}/AM13E_FW1_REL17.flat.bin")
    set(am13e_rel17_manifest "${CMAKE_CURRENT_BINARY_DIR}/AM13E_FW1_REL17.json")
    add_custom_command(OUTPUT "${am13e_rel17_raw}"
        COMMAND "${AM13E_REL17_OBJCOPY}" -O binary --gap-fill=0xff
            "$<TARGET_FILE:AM13E_FW1_REL17.elf>" "${am13e_rel17_raw}"
        DEPENDS AM13E_FW1_REL17.elf
            "${AM13E_PROFILE_APP_LINKER_SCRIPT}"
        VERBATIM)
    add_custom_command(OUTPUT "${am13e_rel17_packed}" "${am13e_rel17_manifest}"
        COMMAND "${Python3_EXECUTABLE}"
            "${AM13E_PROFILE_PACKER_SCRIPT}"
            pack "${am13e_rel17_raw}" "${am13e_rel17_packed}"
            --manifest "${am13e_rel17_manifest}"
        COMMAND "${Python3_EXECUTABLE}"
            "${AM13E_PROFILE_PACKER_SCRIPT}"
            verify "${am13e_rel17_packed}"
        DEPENDS "${am13e_rel17_raw}"
            "${AM13E_PROFILE_PACKER_SCRIPT}"
        VERBATIM)
    add_custom_target(AM13E_FW1_REL17_IMAGE
        DEPENDS "${am13e_rel17_packed}" "${am13e_rel17_manifest}")
endif()
