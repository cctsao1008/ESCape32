# ESCape32 add_target() AM13E backend; TI SDK supplies external libraries.
# E62 now targets original rel17 src/main.c. AM13E peripheral migration is incomplete.
# Do not revive bring-up runtime as production application main.
function(add_target_am13e name)
    include("${CMAKE_CURRENT_SOURCE_DIR}/mcu/AM13E23019/config.cmake")
    set(platform "${CMAKE_CURRENT_SOURCE_DIR}/mcu/AM13E23019")
    file(GLOB generated CONFIGURE_DEPENDS "${AM13E_SYSCFG_DIR}/*.c")
    if(NOT generated)
        message(FATAL_ERROR "AM13E SysConfig sources missing: ${AM13E_SYSCFG_DIR}; generate them using TI SysConfig first")
    endif()
    # CMSIS Core is INTERFACE-only in the TI SDK (headers, no .a).
    # External TI SDK static archives, never configure the SDK as top-level.
    file(GLOB_RECURSE sdk_archives
        "${AM13E_SDK_BUILD}/*.a" "${AM13E_SDK_ROOT}/lib/*.a")
    set(required_libs ti_sdk_cfg_default arch_ti_sdk_cfg_default
        driverlib_ti_sdk_cfg_default utils_nortos_ti_sdk_cfg_default am13e230x)
    set(libs "")
    foreach(lib IN LISTS required_libs)
        set(found "")
        foreach(archive IN LISTS sdk_archives)
            get_filename_component(base "${archive}" NAME_WE)
            if(base STREQUAL "lib${lib}" OR base STREQUAL "${lib}")
                set(found "${archive}")
                break()
            endif()
        endforeach()
        if(NOT found)
            message(FATAL_ERROR "Missing external TI SDK library: ${lib}. Build TI SDK libraries once in ${AM13E_SDK_BUILD}")
        endif()
        list(APPEND libs "${found}")
    endforeach()
    add_executable(${name}.elf
        "${CMAKE_CURRENT_SOURCE_DIR}/src/esc_math.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/esc_config.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/esc_cmd_parse.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/esc_param_metadata.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/esc_param_access.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/prog.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c"
        "${platform}/src/am13e_bemf_events.c"
        "${platform}/src/am13e_timg12.c"
        "${platform}/src/am13e_irq.c"
        ${generated})
    # Prior opt-in probe remains diagnostic-only; the firmware itself now uses main.c.
    option(ESCAPE32_AM13E_REL17_PROBE "Compile rel17 main.c against AM13E headers" OFF)
    if(ESCAPE32_AM13E_REL17_PROBE)
        add_library(${name}_rel17_probe OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
        target_include_directories(${name}_rel17_probe PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src" "${platform}/src")
        target_include_directories(${name}_rel17_probe SYSTEM PRIVATE
            "${AM13E_SYSCFG_DIR}"
            "${AM13E_SDK_ROOT}/source/device/am13e230x/include"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x"
            "${AM13E_SDK_ROOT}/source/arch/include"
            "${AM13E_SDK_ROOT}/source/arch/m33/include"
            "${AM13E_SDK_ROOT}/source/cmsis/Core/Include"
            "${AM13E_SDK_ROOT}/source/device/am13e230x/include/hw"
            "${AM13E_SDK_ROOT}/source/compiler/m33_gcc_arm")
        target_compile_definitions(${name}_rel17_probe PRIVATE
            ESCAPE32_AM13E ESCAPE32_AM13E_MCPWM_INST=MCPWM0
            __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
            __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
        # Probe-only inputs: no values are inferred from unrelated boards.
        foreach(required IN ITEMS CLK DEAD_TIME COMP_MAP)
            if(DEFINED ESCAPE32_AM13E_PROBE_${required})
                target_compile_definitions(${name}_rel17_probe PRIVATE
                    "${required}=${ESCAPE32_AM13E_PROBE_${required}}")
            endif()
        endforeach()
        target_compile_definitions(${name}_rel17_probe PRIVATE TARGET_NAME="E62")
        target_compile_options(${name}_rel17_probe PRIVATE ${am13e_opts})
    endif()
    # Own headers retain all ESCape32 warning diagnostics.
    target_include_directories(${name}.elf PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${platform}/src")
    # Vendor / generated headers are external system headers, like libopencm3.
    target_include_directories(${name}.elf SYSTEM PRIVATE
        "${AM13E_SYSCFG_DIR}"
        "${AM13E_SDK_ROOT}/source/device/am13e230x/include"
        "${AM13E_SDK_ROOT}/source/driverlib/am13e230x"
        "${AM13E_SDK_ROOT}/source/arch/include"
        "${AM13E_SDK_ROOT}/source/arch/m33/include"
        "${AM13E_SDK_ROOT}/source/cmsis/Core/Include"
        "${AM13E_SDK_ROOT}/source/device/am13e230x/include/hw"
        "${AM13E_SDK_ROOT}/source/utils/log/include"
        "${AM13E_SDK_ROOT}/source/utils/utils_delay/include"
        "${AM13E_SDK_ROOT}/source/utils"
        "${AM13E_SDK_ROOT}/ti_sdk_config/am13e230x/default/arch_cfg"
        "${AM13E_SDK_ROOT}/ti_sdk_config/am13e230x/default/arch_cfg/m33"
        "${AM13E_SDK_ROOT}/ti_sdk_config/am13e230x/default/driverlib_cfg"
        "${AM13E_SDK_ROOT}/ti_sdk_config/am13e230x/default/Hal_Cfg"
        "${AM13E_SDK_ROOT}/ti_sdk_config/am13e230x/default/utils_cfg"
        "${AM13E_SDK_ROOT}/source/compiler/m33_gcc_arm"
        "${AM13E_SDK_ROOT}/ti_sdk_config/am13e230x/default/device_support/include")
    target_compile_definitions(${name}.elf PRIVATE
        ESCAPE32_AM13E_PROG_LINK ESCAPE32_AM13E TARGET_NAME="E62"
        # Existing E62 SysConfig/bring-up selects MCPWM0; this is an
        # instance identifier, NOT validated phase pinmux or AQ/dead-time.
        ESCAPE32_AM13E_MCPWM_INST=MCPWM0
        __DEVICE_SHORT__="am13e230x" __DEVICE_LONG__="AM13E230x"
        __CPU_SHORT__="m33" __CGT_SHORT__="gcc_arm")
    # SysConfig C is generated by TI, not ESCape32-owned: isolate its
    # diagnostics without changing strict flags on ESCape32 sources.
    set_source_files_properties(${generated} PROPERTIES COMPILE_OPTIONS "-w")
    target_compile_options(${name}.elf PRIVATE ${am13e_opts})
    target_link_options(${name}.elf PRIVATE ${am13e_opts}
        # Keep rel17 command entry points visible in the ELF even before a
        # transport dispatches them; --gc-sections would otherwise drop them.
        # This does not enable remote commands or storage operations.
        -Wl,-u,execcmd -Wl,-u,execcrsfcmd
        -Wl,--gc-sections -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${name}.map
        -Wl,-e,Reset_Handler -T${platform}/linker_app.ld)
    target_link_libraries(${name}.elf PRIVATE
        -Wl,--start-group ${libs} c_nano m gcc nosys -Wl,--end-group)
    set_target_properties(${name}.elf PROPERTIES OUTPUT_NAME "${name}.elf" SUFFIX "")
    set(am13e_toolchain_root "$ENV{GCC_ARM_TOOLCHAIN_PATH}")
    if(NOT am13e_toolchain_root)
        if(DEFINED ENV{TI_ROOT})
            set(am13e_toolchain_root "$ENV{TI_ROOT}/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi")
        else()
            set(am13e_toolchain_root "$ENV{HOME}/ti/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi")
        endif()
    endif()
    find_program(AM13E_OBJCOPY arm-none-eabi-objcopy
        HINTS "${am13e_toolchain_root}/bin")
    if(NOT AM13E_OBJCOPY)
        message(FATAL_ERROR "arm-none-eabi-objcopy missing; set GCC_ARM_TOOLCHAIN_PATH")
    endif()
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.bin"
        "${CMAKE_CURRENT_BINARY_DIR}/${name}.hex"
        COMMAND "${AM13E_OBJCOPY}" -O binary "$<TARGET_FILE:${name}.elf>" "${CMAKE_CURRENT_BINARY_DIR}/${name}.bin"
        COMMAND "${AM13E_OBJCOPY}" -O ihex "$<TARGET_FILE:${name}.elf>" "${CMAKE_CURRENT_BINARY_DIR}/${name}.hex"
        DEPENDS ${name}.elf VERBATIM)
    add_custom_target(${name} DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/${name}.bin"
        "${CMAKE_CURRENT_BINARY_DIR}/${name}.hex")
    # E62 app image also carries its own boot image header and CRC.
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.e62.bin"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/am13e/pack-am13e-image.py"
          "${CMAKE_CURRENT_BINARY_DIR}/${name}.bin"
          "${CMAKE_CURRENT_BINARY_DIR}/${name}.e62.bin"
          --manifest "${CMAKE_CURRENT_BINARY_DIR}/${name}.e62.json"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${name}.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/am13e/pack-am13e-image.py"
        VERBATIM)
    add_custom_target(${name}-image DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${name}.e62.bin")
endfunction()
