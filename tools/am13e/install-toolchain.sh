#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SDK_DIR="am13e230x_sdk_26_01_00_03"
SDK_RELEASE="26.01.00.03.STS"
SDK_INSTALLER_NAME="am13e230x_sdk_26_01_00_03_STS-linux-x64-installer.run"

EXPECTED_SYSCFG_DIR="sysconfig_1.28.0"
SYSCFG_VERSION_FULL="1.28.0.4712"
SYSCFG_INSTALLER_NAME="sysconfig-1.28.0_4712-setup.run"

EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"
GCC_ARCHIVE_NAME="${EXPECTED_GCC_DIR}.tar.xz"

SDK_URL_DEFAULT="https://dr-download.ti.com/software-development/software-development-kit-sdk/MD-RnlgGIB5Yq/${SDK_RELEASE}/${SDK_INSTALLER_NAME}"
SYSCONFIG_URL_DEFAULT="https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb/${SYSCFG_VERSION_FULL}/${SYSCFG_INSTALLER_NAME}"
ARM_GNU_URL_DEFAULT="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/${GCC_ARCHIVE_NAME}"

SDK_URL="${SDK_URL:-$SDK_URL_DEFAULT}"
SYSCONFIG_URL="${SYSCONFIG_URL:-$SYSCONFIG_URL_DEFAULT}"
ARM_GNU_URL="${ARM_GNU_URL:-$ARM_GNU_URL_DEFAULT}"

TI_ROOT="${TI_ROOT:-$HOME/ti}"
DOWNLOAD_DIR="${DOWNLOAD_DIR:-$PWD}"

SDK_ROOT="${AM13E_SDK_ROOT:-$TI_ROOT/$EXPECTED_SDK_DIR}"
SYSCFG_ROOT="${SYSCFG_PATH:-$TI_ROOT/$EXPECTED_SYSCFG_DIR}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$TI_ROOT/$EXPECTED_GCC_DIR}"

sdk_installer=""
sysconfig_installer=""
gcc_archive=""

install_host_deps=0
skip_sdk=0
skip_gcc=0
skip_sysconfig=0

usage() {
    cat <<'EOF'
Usage:
  install-toolchain.sh [options]

Installs the pinned AM13E development environment below ~/ti by default.

Options:
  --sdk-installer PATH        Use an existing AM13E230x SDK Linux installer.
  --gcc-archive PATH          Use an existing Arm GNU 15.2.rel1 tar.xz archive.
  --sysconfig-installer PATH  Use an existing TI SysConfig 1.28.0 Linux installer.
  --install-host-deps         Install Linux host packages with apt (requires sudo).
  --skip-sdk                  Do not install the AM13E230x SDK.
  --skip-gcc                  Do not install Arm GNU Toolchain.
  --skip-sysconfig            Do not install TI SysConfig.
  -h, --help                  Show this help.

Environment overrides:
  TI_ROOT                     Installation root. Default: ~/ti
  DOWNLOAD_DIR                Download/cache directory. Default: current directory
  SDK_URL                     Override AM13E SDK download URL
  ARM_GNU_URL                 Override Arm GNU Toolchain download URL
  SYSCONFIG_URL               Override SysConfig download URL
  AM13E_SDK_ROOT              Override expected SDK directory
  GCC_ARM_TOOLCHAIN_PATH      Override expected GCC directory
  SYSCFG_PATH                 Override expected SysConfig directory
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --sdk-installer)
            [ "$#" -ge 2 ] || { echo "Missing argument for --sdk-installer" >&2; exit 2; }
            sdk_installer="$2"
            shift 2
            ;;
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
        --skip-sdk)
            skip_sdk=1
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

download_if_missing() {
    local url="$1"
    local path="$2"
    local label="$3"

    if [ -f "$path" ]; then
        echo "Using cached $label: $path"
        return 0
    fi

    echo "Downloading $label to $path..."
    curl -fL --retry 3 --retry-delay 2 -o "$path" "$url"
}

if [ "$skip_sdk" -eq 0 ]; then
    if [ -f "$SDK_ROOT/CMakeLists.txt" ] &&
       [ -f "$SDK_ROOT/.metadata/product.json" ]; then
        echo "AM13E230x SDK already installed: $SDK_ROOT"
    else
        if [ -z "$sdk_installer" ]; then
            sdk_installer="$DOWNLOAD_DIR/$SDK_INSTALLER_NAME"
            download_if_missing "$SDK_URL" "$sdk_installer" "AM13E230x SDK $SDK_RELEASE"
        fi

        [ -f "$sdk_installer" ] || {
            echo "AM13E SDK installer not found: $sdk_installer" >&2
            exit 1
        }

        chmod +x "$sdk_installer"
        echo "Installing AM13E230x SDK into $TI_ROOT..."
        "$sdk_installer" --mode unattended --prefix "$TI_ROOT"

        [ -f "$SDK_ROOT/CMakeLists.txt" ] || {
            echo "SDK installation completed but expected SDK root was not found: $SDK_ROOT" >&2
            exit 1
        }
    fi
fi

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
            download_if_missing "$ARM_GNU_URL" "$gcc_archive" "Arm GNU Toolchain 15.2.rel1"
        fi

        [ -f "$gcc_archive" ] || {
            echo "GCC archive not found: $gcc_archive" >&2
            exit 1
        }

        echo "Installing Arm GNU Toolchain into $TI_ROOT..."
        tar -xJf "$gcc_archive" -C "$TI_ROOT"

        [ -x "$GCC_ROOT/bin/arm-none-eabi-gcc" ] || {
            echo "GCC extraction completed but expected toolchain was not found: $GCC_ROOT" >&2
            exit 1
        }
    fi
fi

if [ "$skip_sysconfig" -eq 0 ]; then
    if [ -d "$SYSCFG_ROOT" ]; then
        echo "SysConfig already installed: $SYSCFG_ROOT"
    else
        if [ -z "$sysconfig_installer" ]; then
            sysconfig_installer="$DOWNLOAD_DIR/$SYSCFG_INSTALLER_NAME"
            download_if_missing "$SYSCONFIG_URL" "$sysconfig_installer" "SysConfig $SYSCFG_VERSION_FULL"
        fi

        [ -f "$sysconfig_installer" ] || {
            echo "SysConfig installer not found: $sysconfig_installer" >&2
            exit 1
        }

        chmod +x "$sysconfig_installer"
        echo "Installing SysConfig into $TI_ROOT..."
        "$sysconfig_installer" --mode unattended --prefix "$TI_ROOT"

        [ -d "$SYSCFG_ROOT" ] || {
            echo "SysConfig installation completed but expected root was not found: $SYSCFG_ROOT" >&2
            exit 1
        }
    fi
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo
"$script_dir/check-env.sh"
