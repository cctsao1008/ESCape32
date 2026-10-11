# Active ESCape32 Rel17 v1.4 flat image profile.
# 488KiB is a region LIMIT, not an image size. No image header/CRC.
set(AM13E_PROFILE_APP_LINKER_SCRIPT
    "${PROJECT_SOURCE_DIR}/mcu/AM13E/config.ld")
set(AM13E_PROFILE_BOOT_LINKER_SCRIPT
    "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/config.ld")
set(AM13E_PROFILE_PACKER_SCRIPT
    "${PROJECT_SOURCE_DIR}/boot/tools/pack_am13e_rel17.py")
foreach(required IN ITEMS
        AM13E_PROFILE_APP_LINKER_SCRIPT
        AM13E_PROFILE_BOOT_LINKER_SCRIPT
        AM13E_PROFILE_PACKER_SCRIPT)
    if(NOT EXISTS "${${required}}")
        message(FATAL_ERROR "Missing REL17_V14 asset ${required}")
    endif()
endforeach()
