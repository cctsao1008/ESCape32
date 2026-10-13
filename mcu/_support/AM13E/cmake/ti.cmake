# A native image has one fixed ABI; retired image modes are not aliases.
if(DEFINED AM13E_IMAGE_PROFILE)
    message(FATAL_ERROR "Unsupported AM13E_IMAGE_PROFILE: image modes are retired; use native config.ld")
endif()
if(AM13E_BOOT_LINKER_SCRIPT AND NOT AM13E_BOOT_LINKER_SCRIPT STREQUAL
   "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/config.ld")
    message(FATAL_ERROR "AM13E Boot must use its native config.ld")
endif()
# TI SDK integration shared by the two native image config.cmake files.
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
    "${PROJECT_SOURCE_DIR}/mcu/_support/AM13E/tools/prepare_sdk_sources.py")
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
# AM13E23019 implements Cortex-M33 / Armv8-M, not the Armv8.1-M
# superset used by TI's generic GCC example. Select the CPU for every
# native object AND link, retaining the single-precision hard-float ABI.
set(opts -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16
    -mfloat-abi=hard -fno-branch-count-reg -Werror)
set(defs __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
    __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
set(core_libs c gcc nosys)
set(libs "")
set(linker_scripts "${CMAKE_CURRENT_SOURCE_DIR}/mcu/AM13E/config.ld")
set(link_opts -Wl,--no-undefined -Wl,--fatal-warnings -specs=nosys.specs
    -Wl,--require-defined=Reset_Handler -Wl,--require-defined=main)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL "${PROJECT_SOURCE_DIR}/boot")
    list(APPEND link_opts -Wl,--require-defined=initio
        -Wl,--require-defined=recvbuf -Wl,--require-defined=sendbuf
        -Wl,--require-defined=crc32 -Wl,--require-defined=write
        -Wl,--require-defined=boot_am13e_launch_application)
else()
    list(APPEND link_opts -Wl,--require-defined=SysTick_Handler
        -Wl,--require-defined=PendSV_Handler -Wl,--require-defined=HardFault_Handler)
endif()

function(am13e_configure_target am13e_binary)
    target_include_directories(${am13e_binary} PRIVATE
        "${PROJECT_SOURCE_DIR}/src/runtime" "${PROJECT_SOURCE_DIR}/src/serial"
        "${CMAKE_CURRENT_SOURCE_DIR}/mcu/AM13E/hal")
    if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL "${PROJECT_SOURCE_DIR}/boot")
        am13e_configure_reference_board(${am13e_binary})
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

function(am13e_pack_image elf bin hex)
    # Build metadata is evidence only; no manifest is added to the wire image.
    set(packer "${PROJECT_SOURCE_DIR}/boot/tools/pack_am13e_rel17.py")
    set(raw "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${elf}.dir/image.raw.bin")
    add_custom_command(OUTPUT ${bin} ${hex}
        COMMAND arm-none-eabi-objcopy -O binary --gap-fill=0xff ${elf} "${raw}"
        COMMAND arm-none-eabi-objcopy -O ihex ${elf} ${hex}
        COMMAND "${Python3_EXECUTABLE}" "${packer}" pack "${raw}" ${bin}
            --manifest "${CMAKE_CURRENT_BINARY_DIR}/${elf}.json"
        COMMAND "${Python3_EXECUTABLE}" "${packer}" verify ${bin}
        DEPENDS ${elf} "${packer}" VERBATIM)
endfunction()
