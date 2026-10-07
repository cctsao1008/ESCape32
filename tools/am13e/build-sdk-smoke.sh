#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
EXPECTED_SYSCFG_DIR="sysconfig_1.28.0"
EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"

TI_ROOT="${TI_ROOT:-$HOME/ti}"
SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"
SYSCFG_ROOT="${SYSCFG_PATH:-$TI_ROOT/$EXPECTED_SYSCFG_DIR}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$TI_ROOT/$EXPECTED_GCC_DIR}"
BUILD_DIR="$SDK_ROOT/build/am13e230x/m33_gcc_arm_debug"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"$script_dir/check-env.sh"

echo
echo "AM13E SDK GCC smoke build"
echo "========================="
echo "SDK root       : $SDK_ROOT"
echo "SysConfig root : $SYSCFG_ROOT"
echo "GCC root       : $GCC_ROOT"
echo "Build dir      : $BUILD_DIR"
echo

# The TI SDK defaults GCC_ARM_TOOLCHAIN_PATH to a toolchain directly below
# $HOME. Our portable layout keeps all AM13E dependencies below ~/ti, so pass
# the pinned paths as command-line make variables. Command-line variables have
# precedence over the SDK's generated imports file.
rm -rf "$BUILD_DIR"

make -C "$SDK_ROOT"     M33_TI_ARM_CLANG_BUILD_ENABLE=n     M33_GCC_ARM_BUILD_ENABLE=y     SYSCFG_PATH="$SYSCFG_ROOT"     GCC_ARM_TOOLCHAIN_PATH="$GCC_ROOT"     cmake-genfiles-m33_gcc_arm_debug

echo
echo "Building TI SDK target: ex_empty_lp"
cmake --build "$BUILD_DIR"     --target ex_empty_lp     --parallel "$(nproc)"

echo
echo "Smoke build completed."
echo "Build artifacts:"
find "$BUILD_DIR" -type f     \( -name 'ex_empty_lp'        -o -name 'ex_empty_lp.*'        -o -name '*ex_empty_lp*.map' \)     -print | sort
