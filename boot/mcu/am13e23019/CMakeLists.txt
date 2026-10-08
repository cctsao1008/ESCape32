# E62 AM13E23019 common bootloader.
set(TARGET am13e23019_e62_boot)
set(TI_SDK_CONFIG ti_sdk_cfg_default)
set(E62_BOOT_DIR ${CMAKE_CURRENT_LIST_DIR})
set(E62_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../mcu/am13e23019)

if(TARGET ${TARGET})
    return()
endif()

if(NOT DEFINED ESCAPE32_AM13E_BOOT_OUTPUT_DIR)
    message(FATAL_ERROR "ESCAPE32_AM13E_BOOT_OUTPUT_DIR is not defined")
endif()

add_executable(${TARGET})

target_sources(${TARGET} PRIVATE
    ${E62_BOOT_DIR}/src/main.c
)

target_include_directories(${TARGET} PRIVATE
    ${E62_BOOT_DIR}
    ${E62_BOOT_DIR}/src
    ${E62_PLATFORM_DIR}
)

target_link_libraries(${TARGET} PRIVATE arch_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE cmsis_core)
target_link_libraries(${TARGET} PRIVATE driverlib_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE utils_nortos_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE ${TI_SDK_DEVICE})

set_target_properties(${TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${ESCAPE32_AM13E_BOOT_OUTPUT_DIR}"
)

target_link_options(${TARGET} PRIVATE
    "${TOOLCHAIN_SET_MAP_PREFIX}${ESCAPE32_AM13E_BOOT_OUTPUT_DIR}/${TARGET}.map"
    "${TOOLCHAIN_SET_LINKER_SCRIPT_PREFIX}${E62_BOOT_DIR}/linker_boot.ld"
)
