# AM13E development tools

These scripts prepare and validate the Linux/WSL host environment used by the
`am13e-port` branch.

The versions are intentionally pinned to the versions expected by
AM13E230x SDK 26.01.00.03:

- AM13E230x SDK: `26_01_00_03`
- SysConfig: `1.28.0.4712`
- Arm GNU Toolchain: `15.2.rel1`

## Default layout

All TI/AM13E development packages live below one Linux-side root:

```text
~/ti/
├── am13e230x_sdk_26_01_00_03/
├── sysconfig_1.28.0/
└── arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/
```

No Windows path is required. This layout is intended to be reproducible on WSL
and native Ubuntu VMs.

Downloaded installers/archives are cached in the current working directory by
default. Run the installer from the repository root if you want the downloads
there. Set `DOWNLOAD_DIR` to use another Linux directory.

## Check the environment

```bash
./tools/am13e/check-env.sh
```

The checker accepts overrides:

```bash
TI_ROOT=/path/to/ti \
AM13E_SDK_ROOT=/path/to/sdk \
SYSCFG_PATH=/path/to/sysconfig \
GCC_ARM_TOOLCHAIN_PATH=/path/to/gcc \
./tools/am13e/check-env.sh
```

## Install the pinned toolchain

Normal use:

```bash
./tools/am13e/install-toolchain.sh
```

On a fresh Ubuntu/WSL installation, host packages can also be installed:

```bash
./tools/am13e/install-toolchain.sh --install-host-deps
```

The installer downloads the pinned Arm GNU and SysConfig installers directly
into the current working directory when they are not already present, then
installs both below `~/ti`.

Existing archives/installers can be supplied explicitly:

```bash
./tools/am13e/install-toolchain.sh \
  --gcc-archive /path/to/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz \
  --sysconfig-installer /path/to/sysconfig-1.28.0_4712-setup.run
```

Useful overrides:

```bash
TI_ROOT=/opt/ti DOWNLOAD_DIR=/tmp/am13e-tools ./tools/am13e/install-toolchain.sh
```

The scripts do not modify the TI SDK.
