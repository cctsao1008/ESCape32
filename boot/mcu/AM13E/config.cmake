# Native Rel17 v1.4 Boot source selection, independent from Application sources.
        set(am13e_sources
            "${PROJECT_SOURCE_DIR}/boot/src/main.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/entry.c"
            "${PROJECT_SOURCE_DIR}/boot/src/io.c"
            "${PROJECT_SOURCE_DIR}/boot/src/util.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/flash_range.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/config.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/io.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/device.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/app.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/flash.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/update_staging.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/update_commit.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/app_validity.c"
            "${PROJECT_SOURCE_DIR}/mcu/AM13E/newlib_syscalls.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_flash.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_flashctl.c")
        set(am13e_source_dir "${PROJECT_SOURCE_DIR}/boot/src")
