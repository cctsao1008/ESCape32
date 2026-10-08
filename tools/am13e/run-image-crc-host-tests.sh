#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O0 -Wall -Wextra -Werror \
  -I"$root/boot/src" \
  "$root/tools/am13e/test_image_crc_host.c" \
  "$root/boot/src/crc32.c" -o "$tmp/test_image_crc_host"
python3 - "$tmp/raw.bin" <<'PY'
import sys
from pathlib import Path
p = Path(sys.argv[1])
p.write_bytes(bytes((i * 13 + 5) & 255 for i in range(4099)))
PY
python3 "$root/tools/am13e/pack-am13e-image.py" \
  "$tmp/raw.bin" "$tmp/packed.bin" > "$tmp/manifest.log"
"$tmp/test_image_crc_host" "$tmp/packed.bin"
python3 - "$tmp/packed.bin" "$tmp/corrupt.bin" <<'PY'
from pathlib import Path
import sys
data = bytearray(Path(sys.argv[1]).read_bytes())
data[512] ^= 1
Path(sys.argv[2]).write_bytes(data)
PY
if "$tmp/test_image_crc_host" "$tmp/corrupt.bin" > /dev/null 2>&1; then
  echo "[FAIL] C Boot CRC accepted corrupt image" >&2
  exit 1
fi
echo "[PASS] C Boot CRC rejects payload corruption"
