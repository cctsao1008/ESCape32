#!/usr/bin/env bash
# Software-only fixtures. No product pin mapping, flash or qualification.
set -euo pipefail
sdk=${1:?official/CI SDK path required}
build_root=${2:?external build root required}
logs=${3:?log path required}
mkdir -p "$logs"
for count in 3 4; do
  cmake -S . -B "$build_root/led$count" -DAM13E_SDK_ROOT="$sdk" \
    -DAM13E_BOARD_BOARD_PROFILE_FILE="$(pwd)/mcu/_support/AM13E/cmake/test_profiles/led${count}_synthetic_profile.cmake" \
    >"$logs/led$count-configure.log" 2>&1
  cmake --build "$build_root/led$count" --target AM13E -j 2 \
    >"$logs/led$count-build.log" 2>&1
  arm-none-eabi-nm "$build_root/led$count/AM13E-rev17.3.elf" >"$logs/led$count-symbols.txt"
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
  "-DLED_WS2812 $route" \
  '-DBEC_MAP=0xB35' \
  '-DBEC_MAP=0xADE' \
  '-DBEC_MAP=0xB35 -DAM13E_BOARD_BEC_BITS=1 -DAM13E_BOARD_BEC1_GPIO=GPIO1 -DAM13E_BOARD_BEC1_MASK=1U -DAM13E_BOARD_BEC1_PINCM=32U' \
  '-DBEC_MAP=0xB35 -DAM13E_BOARD_BEC_BITS=2 -DAM13E_BOARD_BEC1_GPIO=GPIO1 -DAM13E_BOARD_BEC1_MASK=8192U -DAM13E_BOARD_BEC1_PINCM=45U -DAM13E_BOARD_BEC2_GPIO=GPIO1 -DAM13E_BOARD_BEC2_MASK=2U -DAM13E_BOARD_BEC2_PINCM=33U'; do
  if cc -std=c11 -DAM13E -Isrc -Imcu/AM13E -Imcu/_support/AM13E/tests/app/aux_mock \
    -fsyntax-only $flags mcu/AM13E/hal/aux_gpio.c >"$logs/led-negative.log" 2>&1; then
    echo 'FAIL invalid or missing LED capability accepted'; exit 1
  fi
  grep -Eq 'needs explicit AM13E|LED route conflicts|cannot share a pin|remains unimplemented|BEC_MAP needs|BEC on debug|BEC voltage choices|BEC route conflicts' "$logs/led-negative.log"
done
echo 'PASS nine LED/BEC missing/conflicting/unsupported configuration negatives'
