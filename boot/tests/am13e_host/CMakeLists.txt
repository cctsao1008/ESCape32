cmake_minimum_required(VERSION 3.15)
project(AM13EFlashHostTests C)

include(CTest)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
add_executable(am13e_flash_transaction_test
    "${REPO_ROOT}/boot/tests/am13e_flash_transaction_test.c"
    "${REPO_ROOT}/boot/mcu/AM13E/flash.c")
target_compile_features(am13e_flash_transaction_test PRIVATE c_std_11)
target_compile_definitions(am13e_flash_transaction_test PRIVATE AM13E_FLASH_TEST)
target_include_directories(am13e_flash_transaction_test PRIVATE
    "${REPO_ROOT}/boot/tests/am13e_mock")
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
