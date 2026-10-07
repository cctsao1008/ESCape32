#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SYSCFG_DIR="sysconfig_1.28.0"
SYSCFG_VERSION_FULL="1.28.0.4712"
SYSCFG_INSTALLER_NAME="sysconfig-1.28.0_4712-setup.run"

EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"
GCC_ARCHIVE_NAME="${EXPECTED_GCC_DIR}.tar.xz"

ARM_GNU_URL_DEFAULT="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/${GCC_ARCHIVE_NAME}"
SYSCONFIG_URL_DEFAULT="https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb/${SYSCFG_VERSION_FULL}/${SYSCFG_INSTALLER_NAME}"

ARM_GNU_URL="${ARM_GNU_URL:-$ARM_GNU_URL_DEFAULT}"
SYSCONFIG_URL="${SYSCONFIG_URL:-$SYSCONFIG_URL_DEFAULT}"

TI_ROOT="${TI_ROOT:-$HOME/ti}"
DOWNLOAD_DIR="${DOWNLOAD_DIR:-$PWD}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$TI_ROOT/$EXPECTED_GCC_DIR}"
SYSCFG_ROOT="${SYSCFG_PATH:-$TI_ROOT/$EXPECTED_SYSCFG_DIR}"

gcc_archive=""
sysconfig_installer=""
install_host_deps=0
skip_gcc=0
skip_sysconfig=0

usage() {
    cat <<'EOF'
Usage:
  install-toolchain.sh [options]

Options:
  --gcc-archive PATH          Use an existing Arm GNU 15.2.rel1 tar.xz archive.
  --sysconfig-installer PATH  Use an existing TI SysConfig 1.28.0 Linux installer.
  --install-host-deps         Install Linux packages with apt (requires sudo).
  --skip-gcc                  Do not install Arm GNU Toolchain.
  --skip-sysconfig            Do not install TI SysConfig.
  -h, --help                  Show this help.

Environment overrides:
  TI_ROOT
  DOWNLOAD_DIR
  ARM_GNU_URL
  SYSCONFIG_URL
  GCC_ARM_TOOLCHAIN_PATH
  SYSCFG_PATH
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --gcc-archive)
            [ "$#" -ge 2 ] || { echo "Missing argument for --gcc-archive" >&2; exit 2; }
            gcc_archive="$2"
            shift 2
            ;;
        --sysconfig-installer)
            [ "$#" -ge 2 ] || { echo "Missing argument for --sysconfig-installer" >&2; exit 2; }
            sysconfig_installer="$2"
            shift 2
            ;;
        --install-host-deps)
            install_host_deps=1
            shift
            ;;
        --skip-gcc)
            skip_gcc=1
            shift
            ;;
        --skip-sysconfig)
            skip_sysconfig=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

[ "$(uname -s)" = "Linux" ] || { echo "This installer supports Linux/WSL only." >&2; exit 1; }
case "$(uname -m)" in
    x86_64|amd64) ;;
    *) echo "This installer expects an x86_64 host." >&2; exit 1 ;;
esac

if [ "$install_host_deps" -eq 1 ]; then
    sudo apt update
    sudo apt install -y ninja-build graphviz make cmake curl xz-utils python3
fi

command -v curl >/dev/null 2>&1 || {
    echo "curl is required. Install it or rerun with --install-host-deps." >&2
    exit 1
}

mkdir -p "$TI_ROOT" "$DOWNLOAD_DIR"

legacy_gcc_root="$HOME/$EXPECTED_GCC_DIR"
if [ "$skip_gcc" -eq 0 ] &&
   [ ! -x "$GCC_ROOT/bin/arm-none-eabi-gcc" ] &&
   [ -x "$legacy_gcc_root/bin/arm-none-eabi-gcc" ] &&
   [ "$legacy_gcc_root" != "$GCC_ROOT" ]; then
    echo "Migrating existing Arm GNU toolchain into TI root..."
    mv "$legacy_gcc_root" "$GCC_ROOT"
fi

if [ "$skip_gcc" -eq 0 ]; then
    if [ -x "$GCC_ROOT/bin/arm-none-eabi-gcc" ]; then
        echo "Arm GNU toolchain already installed: $GCC_ROOT"
    else
        if [ -z "$gcc_archive" ]; then
            gcc_archive="$DOWNLOAD_DIR/$GCC_ARCHIVE_NAME"
        fi

        if [ ! -f "$gcc_archive" ]; then
            echo "Downloading Arm GNU Toolchain 15.2.rel1 to $gcc_archive..."
            curl -fL --retry 3 -o "$gcc_archive" "$ARM_GNU_URL"
        else
            echo "Using cached Arm GNU archive: $gcc_archive"
        fi

        echo "Installing Arm GNU Toolchain into $TI_ROOT..."
        tar -xJf "$gcc_archive" -C "$TI_ROOT"
    fi
fi

if [ "$skip_sysconfig" -eq 0 ]; then
    if [ -d "$SYSCFG_ROOT" ]; then
        echo "SysConfig already installed: $SYSCFG_ROOT"
    else
        if [ -z "$sysconfig_installer" ]; then
            sysconfig_installer="$DOWNLOAD_DIR/$SYSCFG_INSTALLER_NAME"
        fi

        if [ ! -f "$sysconfig_installer" ]; then
            echo "Downloading SysConfig 1.28.0.4712 to $sysconfig_installer..."
            curl -fL --retry 3 -o "$sysconfig_installer" "$SYSCONFIG_URL"
        else
            echo "Using cached SysConfig installer: $sysconfig_installer"
        fi

        chmod +x "$sysconfig_installer"
        echo "Installing SysConfig into $TI_ROOT..."
        "$sysconfig_installer" --mode unattended --prefix "$TI_ROOT"
    fi
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo
"$script_dir/check-env.sh"
