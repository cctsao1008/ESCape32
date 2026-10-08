# ESCape32 add_target() AM13E backend; TI SDK supplies external libraries.
# Current sources implement a peripheral vertical slice, NOT rel17 src/main.c.
# Full rel17 target must migrate the common.h hardware dependencies first.
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
    set(required_libs arch_ti_sdk_cfg_default driverlib_ti_sdk_cfg_default
        utils_nortos_ti_sdk_cfg_default am13e230x)
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
        "${platform}/src/fw1_runtime.c"
        "${platform}/src/fw1_bemf_events.c"
        "${platform}/src/fw1_timg12.c"
        "${platform}/src/fw1_irq.c"
        ${generated})
    target_include_directories(${name}.elf PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${platform}/src"
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
        "${AM13E_SDK_ROOT}/ti_sdk_config/am13e230x/default/device_support/include")
    target_compile_options(${name}.elf PRIVATE ${am13e_opts})
    target_link_options(${name}.elf PRIVATE ${am13e_opts}
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
