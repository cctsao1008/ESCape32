#!/usr/bin/env bash
set -uo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
EXPECTED_SYSCFG_DIR="sysconfig_1.28.0"
EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"

TI_ROOT="${TI_ROOT:-$HOME/ti}"
SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"
SYSCFG_ROOT="${SYSCFG_PATH:-$TI_ROOT/$EXPECTED_SYSCFG_DIR}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$TI_ROOT/$EXPECTED_GCC_DIR}"

failures=0
warnings=0

pass() { printf '[PASS] %s\n' "$*"; }
warn() { printf '[WARN] %s\n' "$*"; warnings=$((warnings + 1)); }
fail() { printf '[FAIL] %s\n' "$*"; failures=$((failures + 1)); }

version_ge() {
    [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]
}

printf 'AM13E environment check\n'
printf '=======================\n'
printf 'TI root        : %s\n' "$TI_ROOT"
printf 'SDK root       : %s\n' "$SDK_ROOT"
printf 'SysConfig root : %s\n' "$SYSCFG_ROOT"
printf 'GCC root       : %s\n\n' "$GCC_ROOT"

if [ "$(uname -s)" = "Linux" ]; then
    pass "Host OS is Linux"
else
    fail "Host OS is not Linux: $(uname -s)"
fi

case "$(uname -m)" in
    x86_64|amd64) pass "Host architecture is x86_64" ;;
    *) fail "Expected x86_64 host, found $(uname -m)" ;;
esac

if grep -qi microsoft /proc/version 2>/dev/null; then
    pass "WSL detected"
else
    warn "WSL not detected; native Linux is also supported"
fi

for tool in make ninja python3 tar xz; do
    if command -v "$tool" >/dev/null 2>&1; then
        pass "$tool: $(command -v "$tool")"
    else
        fail "$tool not found"
    fi
done

if command -v cmake >/dev/null 2>&1; then
    cmake_ver="$(cmake --version | awk 'NR==1 {print $3}')"
    if version_ge "$cmake_ver" "3.15"; then
        pass "cmake $cmake_ver"
    else
        fail "cmake $cmake_ver found; AM13E SDK requires >= 3.15"
    fi
else
    fail "cmake not found"
fi

if [ -f "$SDK_ROOT/CMakeLists.txt" ] &&
   [ -f "$SDK_ROOT/.metadata/product.json" ] &&
   [ -f "$SDK_ROOT/cmake/toolchain/toolchain_m33_gcc_arm.cmake" ]; then
    pass "AM13E SDK structure found"
else
    fail "AM13E SDK not found or incomplete at $SDK_ROOT"
fi

if [ -d "$SYSCFG_ROOT" ]; then
    if [ -x "$SYSCFG_ROOT/sysconfig_cli.sh" ] ||
       [ -f "$SYSCFG_ROOT/dist/cli.js" ] ||
       [ -f "$SYSCFG_ROOT/nodejs/node" ]; then
        pass "SysConfig 1.28.0 installation found"
    else
        warn "SysConfig directory exists but expected CLI files were not recognized"
    fi
else
    fail "SysConfig 1.28.0 not found at $SYSCFG_ROOT"
fi

gcc_bin="$GCC_ROOT/bin/arm-none-eabi-gcc"
if [ -x "$gcc_bin" ]; then
    gcc_version="$("$gcc_bin" -dumpfullversion 2>/dev/null || "$gcc_bin" -dumpversion 2>/dev/null || true)"
    case "$gcc_version" in
        15.2*) pass "Arm GNU GCC $gcc_version from pinned 15.2.rel1 toolchain path" ;;
        *) warn "GCC exists at pinned path but reports version '$gcc_version'" ;;
    esac
else
    legacy_gcc="$HOME/$EXPECTED_GCC_DIR"
    if [ -x "$legacy_gcc/bin/arm-none-eabi-gcc" ]; then
        fail "Pinned Arm GNU toolchain is in legacy location $legacy_gcc; move it to $GCC_ROOT"
    else
        fail "Pinned Arm GNU toolchain not found at $GCC_ROOT"
    fi
fi

printf '\n'
if [ "$failures" -eq 0 ]; then
    printf 'Environment ready'
    if [ "$warnings" -gt 0 ]; then
        printf ' (%d warning(s))' "$warnings"
    fi
    printf '.\n'
    exit 0
fi

printf 'Environment is not ready: %d failure(s), %d warning(s).\n' "$failures" "$warnings"
printf 'Run tools/am13e/install-toolchain.sh or install the missing dependencies manually.\n'
exit 1
