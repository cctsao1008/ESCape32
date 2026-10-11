#!/usr/bin/env bash
# Software-only fixtures. No product pin mapping, flash or qualification.
set -euo pipefail
sdk=${1:?official/CI SDK path required}
build_root=${2:?external build root required}
logs=${3:?log path required}
mkdir -p "$logs"
for count in 3 4; do
  cmake -S . -B "$build_root/led$count" -DAM13E_SDK_ROOT="$sdk" \
    -DAM13E_BOARD_BOARD_PROFILE_FILE="$(pwd)/mcu/AM13E/tests/led${count}_synthetic_profile.cmake" \
    >"$logs/led$count-configure.log" 2>&1
  cmake --build "$build_root/led$count" --target AM13E_FW1_REL17_IMAGE -j 2 \
    >"$logs/led$count-build.log" 2>&1
  arm-none-eabi-nm "$build_root/led$count/AM13E_FW1_REL17.elf" >"$logs/led$count-symbols.txt"
  grep -Eq ' [Tt] initled$' "$logs/led$count-symbols.txt"
  grep -Eq ' [Tt] ledctl$' "$logs/led$count-symbols.txt"
  echo "PASS synthetic $count discrete LED strict ARM link; NOT FLASHED"
done
route='-DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_LED1_MASK=1U -DAM13E_BOARD_LED1_PINCM=32U'
for flags in \
  '-DLED_MAP=0xAF' \
  '-DLED_MAP=0xAF -DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_LED1_MASK=8192U -DAM13E_BOARD_LED1_PINCM=45U' \
  '-DLED_MAP=0xAF -DAM13E_BOARD_LED1_GPIO=GPIO1 -DAM13E_BOARD_LED1_MASK=8192U -DAM13E_BOARD_LED1_PINCM=32U' \
  "-DLED_MAP=0xF2AF $route -DAM13E_BOARD_LED2_GPIO=GPIO1 -DAM13E_BOARD_LED2_MASK=1U -DAM13E_BOARD_LED2_PINCM=32U" \
  "-DLED_MAP=0xAF -DLED_OD $route" \
  "-DLED_WS2812 $route"; do
  if cc -std=c11 -DAM13E -Isrc -Imcu/AM13E -Imcu/AM13E/tests/aux_mock \
    -fsyntax-only $flags mcu/AM13E/aux_gpio.c >"$logs/led-negative.log" 2>&1; then
    echo 'FAIL invalid or missing LED capability accepted'; exit 1
  fi
  grep -Eq 'needs explicit AM13E|LED route conflicts|cannot share a pin|remains unimplemented' "$logs/led-negative.log"
done
echo 'PASS six missing/conflicting/unsupported LED configuration negatives'
