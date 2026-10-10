# SUPERSEDED: historical/unselected REFERENCE_V16 profile artifact.
# DO NOT select this profile for current Rel17 v1.4 linked firmware.
# Current source: mcu/AM13E/profiles/image_rel17_v14.cmake.
# Existing AM13E REFERENCE_V16 image ABI selector (NOT universal MCU policy).
# No change to linker layouts, on-flash magic, signature, CRC or packer.
set(AM13E_PROFILE_APP_LINKER_SCRIPT
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/linker_app_v16.ld")
set(AM13E_PROFILE_BOOT_LINKER_SCRIPT
    "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/linker_boot_reference.ld")
set(AM13E_PROFILE_PACKER_SCRIPT
    "${PROJECT_SOURCE_DIR}/boot/tools/pack_am13e_v2.py")
foreach(required IN ITEMS
        AM13E_PROFILE_APP_LINKER_SCRIPT
        AM13E_PROFILE_BOOT_LINKER_SCRIPT
        AM13E_PROFILE_PACKER_SCRIPT)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "Missing REFERENCE_V16 asset ${required}")
    endif()
endforeach()
