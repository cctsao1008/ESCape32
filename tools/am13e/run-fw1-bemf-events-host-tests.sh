#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"${CC:-cc}" -std=c11 -O0 -Wall -Wextra -Werror -I"$root/src" \
 "$root/tools/am13e/test-fw1-bemf-events-host.c" -o "$tmp/test-fw1-bemf-events"
"$tmp/test-fw1-bemf-events"
