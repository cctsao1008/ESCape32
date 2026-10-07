#!/usr/bin/env bash
set -euo pipefail

LIBOPENCM3_URL="https://github.com/libopencm3/libopencm3.git"
LIBOPENCM3_COMMIT="${LIBOPENCM3_COMMIT:-ddcbb2889fa018626c604e27ea50e66db5755b78}"
LIBOPENCM3_TARGETS="stm32/f0 stm32/g0 stm32/g4 stm32/l4"

EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"
TI_ROOT="${TI_ROOT:-$HOME/ti}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$TI_ROOT/$EXPECTED_GCC_DIR}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
deps_root="${ESCAPE32_DEPS_ROOT:-$repo_root/.deps}"
libopencm3_dir="${LIBOPENCM3_DIR:-$deps_root/libopencm3}"
build_dir="${ESCAPE32_LEGACY_BUILD_DIR:-$repo_root/.am13e-build/legacy-regression}"

export PATH="$GCC_ROOT/bin:$PATH"

echo "ESCape32 legacy regression build"
echo "================================"
echo "Repository       : $repo_root"
echo "GCC root         : $GCC_ROOT"
echo "libopencm3       : $libopencm3_dir"
echo "libopencm3 commit: $LIBOPENCM3_COMMIT"
echo "Build dir        : $build_dir"
echo

command -v arm-none-eabi-gcc >/dev/null || {
    echo "arm-none-eabi-gcc not found under $GCC_ROOT/bin" >&2
    exit 1
}

echo "Compiler:"
arm-none-eabi-gcc --version | head -n 1
echo

mkdir -p "$deps_root"

if [ ! -d "$libopencm3_dir/.git" ]; then
    echo "Cloning libopencm3..."
    git clone "$LIBOPENCM3_URL" "$libopencm3_dir"
fi

echo "Checking out pinned libopencm3 revision..."
git -C "$libopencm3_dir" fetch --quiet origin "$LIBOPENCM3_COMMIT" || true
git -C "$libopencm3_dir" checkout --quiet --detach "$LIBOPENCM3_COMMIT"

actual_commit="$(git -C "$libopencm3_dir" rev-parse HEAD)"
if [ "$actual_commit" != "$LIBOPENCM3_COMMIT" ]; then
    echo "libopencm3 revision mismatch: expected $LIBOPENCM3_COMMIT, got $actual_commit" >&2
    exit 1
fi

echo "Building libopencm3 targets: $LIBOPENCM3_TARGETS"
make -C "$libopencm3_dir" -j"$(nproc)" TARGETS="$LIBOPENCM3_TARGETS"

rm -rf "$build_dir"

echo
echo "Configuring ESCape32 legacy build..."
cmake     -S "$repo_root"     -B "$build_dir"     -D LIBOPENCM3_DIR="$libopencm3_dir"

echo
echo "Building STM32G071 regression target: ESCAPE1"
cmake --build "$build_dir" --target ESCAPE1 --parallel "$(nproc)"

echo
echo "Building STM32G431 regression target: PHOTONDRIVE1"
cmake --build "$build_dir" --target PHOTONDRIVE1 --parallel "$(nproc)"

echo
echo "Legacy regression PASS."
echo "Validated:"
echo "  ESCAPE1       (STM32G071)"
echo "  PHOTONDRIVE1  (STM32G431)"
