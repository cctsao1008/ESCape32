#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O0 -Wall -Wextra -Werror \
  -I"$root/boot/src" \
  -I"$root/boot/mcu/AM13E23019/src" \
  -I"$root/mcu/AM13E23019" \
  "$root/tools/am13e/test_boot_image_host.c" \
  "$root/boot/src/crc32.c" \
  -o "$tmp/test_boot_image_host"
python3 - "$tmp/raw.bin" <<'PY'
from pathlib import Path
import sys
image = bytearray((i * 17 + 3) & 255 for i in range(4099))
image[0:4] = (0x20001000).to_bytes(4, "little")
image[4:8] = (0x00006181).to_bytes(4, "little")
Path(sys.argv[1]).write_bytes(image)
PY
python3 "$root/tools/am13e/pack-am13e-image.py" \
  "$tmp/raw.bin" "$tmp/packed.e62.bin" > /dev/null
"$tmp/test_boot_image_host" "$tmp/packed.e62.bin"
