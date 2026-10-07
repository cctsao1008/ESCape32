# Stage A: AM13E23019 build-only bring-up target.
#
# This CMakeLists.txt is added to the pinned TI SDK top-level CMake project by
# tools/am13e/build-am13e-bringup.sh. It intentionally uses TI SDK libraries,
# SysConfig and the TI default GCC linker script.
#
# The application vector base is overridden to 0x00006000. This validates the
# Boot/Config/Application flash contract without changing ESCape32 motor logic.

set(TARGET am13e23019_bringup)
set(TI_SDK_CONFIG ti_sdk_cfg_default)

get_target_property(LINKER_SCRIPT ${TI_SDK_CONFIG} LINKER_SCRIPT_DEFAULT)

add_executable(${TARGET})

target_sources(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

ti_sdk_add_syscfg_file(
    ${TARGET}
    ${CMAKE_CURRENT_SOURCE_DIR}/example.syscfg
)

target_link_libraries(${TARGET} PRIVATE arch_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE cmsis_core)
target_link_libraries(${TARGET} PRIVATE driverlib_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE utils_nortos_${TI_SDK_CONFIG})
target_link_libraries(${TARGET} PRIVATE ${TI_SDK_DEVICE})

# The TI default linker script exposes _intvecs_base_address as an overridable
# symbol. Stage A uses that mechanism so the first allocatable flash section is
# the application vector table at 0x6000. A dedicated linker script can replace
# this once Boot/Config ownership and flash-update behavior are validated.
target_link_options(${TARGET} PRIVATE
    "-Wl,--defsym,_intvecs_base_address=0x00006000"
)

ti_sdk_set_linker_script_and_map_file(${TARGET} ${LINKER_SCRIPT})
