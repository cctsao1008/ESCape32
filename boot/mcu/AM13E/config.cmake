# Native Rel17 v1.4 Boot source selection, independent from Application sources.
        set(am13e_sources
            "${PROJECT_SOURCE_DIR}/boot/src/main.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/config.c"
            "${PROJECT_SOURCE_DIR}/boot/src/io.c"
            "${PROJECT_SOURCE_DIR}/boot/src/util.c"
            "${PROJECT_SOURCE_DIR}/boot/src/flash_range.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/io.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/device.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/app.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/handoff.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/flash.c"
            "${PROJECT_SOURCE_DIR}/boot/src/update_staging.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/update_commit.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/wrp_nonmain.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/wrp_nonmain_runtime.c"
            "${PROJECT_SOURCE_DIR}/boot/mcu/AM13E/hal/app_validity.c"
            "${PROJECT_SOURCE_DIR}/src/newlib_syscalls.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_flash.c"
            "${AM13E_SDK_ROOT}/source/driverlib/am13e230x/dl_flashctl.c")
        set(am13e_source_dir "${PROJECT_SOURCE_DIR}/boot/src")
