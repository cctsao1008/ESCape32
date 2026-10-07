#!/usr/bin/env bash
set -euo pipefail

EXPECTED_SYSCFG_DIR="sysconfig_1.28.0"
EXPECTED_GCC_DIR="arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi"
GCC_ARCHIVE_NAME="${EXPECTED_GCC_DIR}.tar.xz"

# Arm's historical official binary-release URL. Override with ARM_GNU_URL if
# the release location changes.
ARM_GNU_URL_DEFAULT="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/${GCC_ARCHIVE_NAME}"
ARM_GNU_URL="${ARM_GNU_URL:-$ARM_GNU_URL_DEFAULT}"

GCC_INSTALL_ROOT="${GCC_INSTALL_ROOT:-$HOME}"
SYSCFG_INSTALL_ROOT="${SYSCFG_INSTALL_ROOT:-$HOME/ti}"
GCC_ROOT="${GCC_ARM_TOOLCHAIN_PATH:-$GCC_INSTALL_ROOT/$EXPECTED_GCC_DIR}"
SYSCFG_ROOT="${SYSCFG_PATH:-$SYSCFG_INSTALL_ROOT/$EXPECTED_SYSCFG_DIR}"

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
  --sysconfig-installer PATH  Use the TI SysConfig 1.28.0 Linux .run installer.
  --install-host-deps         Install Linux packages with apt (requires sudo).
  --skip-gcc                  Do not install Arm GNU Toolchain.
  --skip-sysconfig            Do not install TI SysConfig.
  -h, --help                  Show this help.

Environment overrides:
  ARM_GNU_URL
  GCC_INSTALL_ROOT
  SYSCFG_INSTALL_ROOT
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

find_gcc_archive() {
    local candidate
    for candidate in         "$PWD/$GCC_ARCHIVE_NAME"         "$HOME/Downloads/$GCC_ARCHIVE_NAME"         /mnt/c/Users/*/Downloads/"$GCC_ARCHIVE_NAME"; do
        if [ -f "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

find_sysconfig_installer() {
    local candidate
    for candidate in         "$PWD"/sysconfig-1.28.0_*-setup.run         "$HOME"/Downloads/sysconfig-1.28.0_*-setup.run         /mnt/c/Users/*/Downloads/sysconfig-1.28.0_*-setup.run; do
        if [ -f "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

if [ "$skip_gcc" -eq 0 ]; then
    if [ -x "$GCC_ROOT/bin/arm-none-eabi-gcc" ]; then
        echo "Arm GNU toolchain already installed: $GCC_ROOT"
    else
        if [ -z "$gcc_archive" ]; then
            gcc_archive="$(find_gcc_archive || true)"
        fi

        if [ -z "$gcc_archive" ]; then
            command -v curl >/dev/null 2>&1 || {
                echo "curl is required to download Arm GNU Toolchain." >&2
                echo "Install it or pass --gcc-archive PATH." >&2
                exit 1
            }
            tmpdir="$(mktemp -d)"
            trap 'rm -rf "$tmpdir"' EXIT
            gcc_archive="$tmpdir/$GCC_ARCHIVE_NAME"
            echo "Downloading Arm GNU Toolchain 15.2.rel1..."
            curl -fL --retry 3 -o "$gcc_archive" "$ARM_GNU_URL"
        fi

        [ -f "$gcc_archive" ] || { echo "GCC archive not found: $gcc_archive" >&2; exit 1; }
        mkdir -p "$GCC_INSTALL_ROOT"
        echo "Installing Arm GNU Toolchain into $GCC_INSTALL_ROOT..."
        tar -xJf "$gcc_archive" -C "$GCC_INSTALL_ROOT"
    fi
fi

if [ "$skip_sysconfig" -eq 0 ]; then
    if [ -d "$SYSCFG_ROOT" ]; then
        echo "SysConfig already installed: $SYSCFG_ROOT"
    else
        if [ -z "$sysconfig_installer" ]; then
            sysconfig_installer="$(find_sysconfig_installer || true)"
        fi

        if [ -z "$sysconfig_installer" ]; then
            cat >&2 <<EOF
TI SysConfig 1.28.0 installer was not found.

Download the Linux SysConfig 1.28.0 installer from TI and rerun:
  $0 --sysconfig-installer /path/to/sysconfig-1.28.0_xxxx-setup.run
EOF
            exit 1
        fi

        [ -f "$sysconfig_installer" ] || {
            echo "SysConfig installer not found: $sysconfig_installer" >&2
            exit 1
        }

        chmod +x "$sysconfig_installer"
        mkdir -p "$SYSCFG_INSTALL_ROOT"
        echo "Installing SysConfig into $SYSCFG_INSTALL_ROOT..."
        "$sysconfig_installer" --mode unattended --prefix "$SYSCFG_INSTALL_ROOT"
    fi
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo
"$script_dir/check-env.sh"
