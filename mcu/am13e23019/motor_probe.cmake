# AM13E23019 Stage-B MCPWM backend compile probe.
#
# Included by tools/am13e/build-am13e-motor-probe.sh after the pinned TI SDK
# has generated the SysConfig output for its official MCPWM global-load example.

set(TARGET am13e23019_motor_probe)
set(TI_SDK_CONFIG ti_sdk_cfg_default)
set(AM13E_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR})
set(
    AM13E_MCPWM_SYSCFG_DIR
    ${CMAKE_SOURCE_DIR}/examples/driverlib/mcpwm/mcpwm_global_load_use_case/am13e230x_lp/m33_nortos/cmake_syscfg_generated
)

if(TARGET ${TARGET})
    return()
endif()

if(NOT DEFINED ESCAPE32_AM13E_MOTOR_PROBE_OUTPUT_DIR)
    message(FATAL_ERROR "ESCAPE32_AM13E_MOTOR_PROBE_OUTPUT_DIR is not defined")
endif()

file(GLOB AM13E_MCPWM_SYSCFG_SOURCES CONFIGURE_DEPENDS
    "${AM13E_MCPWM_SYSCFG_DIR}/*.c"
)

if(NOT AM13E_MCPWM_SYSCFG_SOURCES)
    message(FATAL_ERROR
        "TI MCPWM global-load SysConfig output not found in "
        "${AM13E_MCPWM_SYSCFG_DIR}. Build the TI example first."
    )
endif()

get_target_property(LINKER_SCRIPT ${TI_SDK_CONFIG} LINKER_SCRIPT_DEFAULT)

add_executable(${TARGET})

target_sources(${TARGET} PRIVATE
    ${AM13E_PLATFORM_DIR}/src/motor_probe.c
    ${AM13E_MCPWM_SYSCFG_SOURCES}
)

target_include_directories(${TARGET} PRIVATE
    ${AM13E_PLATFORM_DIR}
    ${AM13E_PLATFORM_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../../src
    ${AM13E_MCPWM_SYSCFG_DIR}
)

target_link_libraries(${TARGET} PRIVATE arch_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE cmsis_core)
target_link_libraries(${TARGET} PRIVATE driverlib_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE utils_nortos_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE ${TI_SDK_DEVICE})

set_target_properties(${TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ESCAPE32_AM13E_MOTOR_PROBE_OUTPUT_DIR}"
)

target_link_options(${TARGET} PRIVATE
    "-Wl,--defsym,_intvecs_base_address=0x00006000"
    "${TOOLCHAIN_SET_MAP_PREFIX}${ESCAPE32_AM13E_MOTOR_PROBE_OUTPUT_DIR}/${TARGET}.map"
    "${TOOLCHAIN_SET_LINKER_SCRIPT_PREFIX}${LINKER_SCRIPT}"
)
