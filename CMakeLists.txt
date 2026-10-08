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
    set(AM13E_SYSCFG_DIR
        "${AM13E_SDK_ROOT}/examples/empty/am13e230x_lp/m33_nortos/cmake_syscfg_generated"
        CACHE PATH "AM13E SysConfig generated headers (temporary)")
    # This target compiles the original rel17 sources. No board clock,
    # dead time, comparator routing or output-enable values are fabricated.
    if(name MATCHES "^BOOT")
        set(am13e_sources
            boot/src/main.c
            boot/src/io.c
            boot/src/util.c)
        set(am13e_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/boot/src")
    else()
        set(am13e_sources
            src/main.c
            src/io.c
            src/telem.c
            src/util.c
            src/prog.c)
        set(am13e_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")
    endif()
    add_library(${name} OBJECT ${am13e_sources})
    target_compile_definitions(${name} PRIVATE
        AM13E TARGET_NAME="${name}"
        __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
        __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
    target_compile_options(${name} PRIVATE
        -march=armv8.1-m.main -mthumb -mfpu=fpv5-sp-d16
        -mfloat-abi=hard -fno-branch-count-reg)
    target_include_directories(${name} PRIVATE "${am13e_source_dir}")
    target_include_directories(${name} SYSTEM PRIVATE
        "${AM13E_SYSCFG_DIR}"
        "${AM13E_SDK_ROOT}/source/device/am13e230x/include"
        "${AM13E_SDK_ROOT}/source/driverlib/am13e230x"
        "${AM13E_SDK_ROOT}/source/arch/include"
        "${AM13E_SDK_ROOT}/source/arch/m33/include"
        "${AM13E_SDK_ROOT}/source/cmsis/Core/Include"
        "${AM13E_SDK_ROOT}/source/compiler/m33_gcc_arm")
endfunction()

function(add_target name mcu)
    if(mcu STREQUAL "AM13E")
        add_target_ti_am13e(${name})
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
