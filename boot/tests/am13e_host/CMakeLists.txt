cmake_minimum_required(VERSION 3.15)
project(AM13EFlashHostTests C)

include(CTest)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
add_executable(am13e_flash_transaction_test
    "${REPO_ROOT}/boot/tests/am13e_flash_transaction_test.c"
    "${REPO_ROOT}/boot/mcu/AM13E/flash.c"
    "${REPO_ROOT}/boot/mcu/AM13E/app_validity.c")
target_compile_features(am13e_flash_transaction_test PRIVATE c_std_11)
target_compile_definitions(am13e_flash_transaction_test PRIVATE AM13E_FLASH_TEST)
target_include_directories(am13e_flash_transaction_test PRIVATE
    "${REPO_ROOT}/boot/tests/am13e_mock"
    "${REPO_ROOT}/boot/mcu/AM13E")
target_compile_options(am13e_flash_transaction_test PRIVATE
    -O1 -g -Wall -Wextra -Werror)
option(AM13E_HOST_SANITIZERS "Enable AddressSanitizer and UBSan" ON)
if(AM13E_HOST_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(am13e_flash_transaction_test PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(am13e_flash_transaction_test PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME am13e_flash_transaction COMMAND am13e_flash_transaction_test)

add_executable(am13e_app_validity_test
    "${REPO_ROOT}/boot/tests/am13e_app_validity_test.c"
    "${REPO_ROOT}/boot/mcu/AM13E/app_validity.c")
target_compile_features(am13e_app_validity_test PRIVATE c_std_11)
target_include_directories(am13e_app_validity_test PRIVATE
    "${REPO_ROOT}/boot/mcu/AM13E")
target_compile_options(am13e_app_validity_test PRIVATE
    -O1 -g -Wall -Wextra -Werror)
if(AM13E_HOST_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(am13e_app_validity_test PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(am13e_app_validity_test PRIVATE
        -fsanitize=address,undefined)
endif()
add_test(NAME am13e_app_validity COMMAND am13e_app_validity_test)
# Rel17 v1.4 has no image CRC or signature-last early-finalize gate.
# Stage C: packer CLI uses Python's stdlib and a separate temporary image.
# Keep it under CTest alongside the ARM-independent firmware integrity tests.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME am13e_image_packer
    COMMAND "${Python3_EXECUTABLE}" "${REPO_ROOT}/boot/tests/am13e_image_packer_test.py")

# Stage C3 is optional because the ARM-linked smoke image lives in a
# separate CMake build directory, not in this native-GCC CTest tree.
# Pass the actual absolute path. Never silently regenerate a synthetic image.
set(AM13E_HOST_PACKED_IMAGE "" CACHE FILEPATH
    "Real ARM ELF-derived Rel17 flat application image")
if(AM13E_HOST_PACKED_IMAGE)
    if(NOT EXISTS "${AM13E_HOST_PACKED_IMAGE}")
        message(FATAL_ERROR
            "AM13E_HOST_PACKED_IMAGE does not exist: ${AM13E_HOST_PACKED_IMAGE}")
    endif()
    add_test(NAME am13e_linked_image_transaction
        COMMAND am13e_flash_transaction_test
            --flash-image "${AM13E_HOST_PACKED_IMAGE}")

    # Stage D1: compile actual shared ESCape32 main.c / io.c framing,
    # real AM13E Flash-range and Flash-write paths, with mock PB14/DriverLib.
    add_executable(am13e_boot_protocol_test
        "${REPO_ROOT}/boot/tests/am13e_protocol_host_test.c"
        "${REPO_ROOT}/boot/tests/am13e_protocol_entry.c"
        "${REPO_ROOT}/boot/src/io.c"
        "${REPO_ROOT}/boot/mcu/AM13E/flash_range.c"
        "${REPO_ROOT}/boot/mcu/AM13E/flash.c"
        "${REPO_ROOT}/boot/mcu/AM13E/app_validity.c")
    target_compile_features(am13e_boot_protocol_test PRIVATE c_std_11)
    target_compile_definitions(am13e_boot_protocol_test PRIVATE
        AM13E AM13E_FLASH_TEST IO_PB14)
    target_include_directories(am13e_boot_protocol_test PRIVATE
        "${REPO_ROOT}/boot/src"
        "${REPO_ROOT}/boot/mcu/AM13E"
        "${REPO_ROOT}/boot/tests/am13e_mock")
    # Force the platform's most error-prone signedness. The 0xff
    # 1-KiB count must decode correctly regardless of target ABI.
    target_compile_options(am13e_boot_protocol_test PRIVATE
        -O1 -g -Wall -Wextra -Werror -fsigned-char)
    if(AM13E_HOST_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(am13e_boot_protocol_test PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(am13e_boot_protocol_test PRIVATE
            -fsanitize=address,undefined)
    endif()
    add_test(NAME am13e_boot_protocol
        COMMAND am13e_boot_protocol_test "${AM13E_HOST_PACKED_IMAGE}")

    # Actual dispatcher + Flash writer + metadata commit across logical
    # block255->256, not merely a host-only address arithmetic test.
    set(AM13E_WINDOW1_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/am13e-window1-packed.bin")
    add_test(NAME am13e_window1_image_fixture
        COMMAND "${Python3_EXECUTABLE}"
            "${REPO_ROOT}/boot/tests/am13e_window1_image_fixture.py"
            "${AM13E_HOST_PACKED_IMAGE}" "${AM13E_WINDOW1_IMAGE}")
    set_tests_properties(am13e_window1_image_fixture PROPERTIES
        FIXTURES_SETUP am13e_window1_fixture)
    add_test(NAME am13e_boot_protocol_window1
        COMMAND am13e_boot_protocol_test "${AM13E_WINDOW1_IMAGE}")
    set_tests_properties(am13e_boot_protocol_window1 PROPERTIES
        FIXTURES_REQUIRED am13e_window1_fixture)

    # Full 488 KiB end-exclusive boundary, including block 487
    # and window1 slot 231, through actual Boot parser+Flash writer.
    set(AM13E_FULL_IMAGE "${CMAKE_CURRENT_BINARY_DIR}/am13e-full488-packed.bin")
    add_test(NAME am13e_full488_image_fixture
        COMMAND "${Python3_EXECUTABLE}"
            "${REPO_ROOT}/boot/tests/am13e_window1_image_fixture.py"
            "${AM13E_HOST_PACKED_IMAGE}" "${AM13E_FULL_IMAGE}" --full)
    set_tests_properties(am13e_full488_image_fixture PROPERTIES
        FIXTURES_SETUP am13e_full488_fixture)
    add_test(NAME am13e_boot_protocol_full488
        COMMAND am13e_boot_protocol_test "${AM13E_FULL_IMAGE}")
    set_tests_properties(am13e_boot_protocol_full488 PROPERTIES
        FIXTURES_REQUIRED am13e_full488_fixture)
endif()
