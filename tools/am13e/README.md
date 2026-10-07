# AM13E development tools

These scripts prepare and validate the Linux/WSL host environment used by the
`am13e-port` branch.

The environment is pinned to the versions expected by AM13E230x SDK
26.01.00.03:

- AM13E230x SDK: `26.01.00.03.STS`
- SysConfig: `1.28.0.4712`
- Arm GNU Toolchain: `15.2.rel1`

## Default layout

All AM13E development packages are installed below one Linux-native root:

```text
~/ti/
├── am13e230x_sdk_26_01_00_03/
├── sysconfig_1.28.0/
└── arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/
```

This layout is intended to work the same way on WSL and native Ubuntu VMs.
Windows paths are not used.

Installer/archive files are cached in the current working directory by default.
When the script is run from the repository root, the downloads therefore remain
next to the repository build environment while the installed tools live under
`~/ti`. Set `DOWNLOAD_DIR` to choose another Linux directory.

## Bootstrap a fresh machine

From the repository root:

```bash
./tools/am13e/install-toolchain.sh --install-host-deps
```

If the Linux host dependencies already exist:

```bash
./tools/am13e/install-toolchain.sh
```

The script installs or validates all three pinned components:

1. AM13E230x SDK 26.01.00.03.STS
2. Arm GNU Toolchain 15.2.rel1
3. SysConfig 1.28.0.4712

Existing installations are reused. Existing downloaded installers/archives are
also reused.

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

## Use local installer files

The bootstrap script can use existing files instead of downloading them:

```bash
./tools/am13e/install-toolchain.sh \
  --sdk-installer /path/to/am13e230x_sdk_26_01_00_03_STS-linux-x64-installer.run \
  --gcc-archive /path/to/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz \
  --sysconfig-installer /path/to/sysconfig-1.28.0_4712-setup.run
```

Useful environment overrides:

```bash
TI_ROOT=/opt/ti DOWNLOAD_DIR=/tmp/am13e-tools ./tools/am13e/install-toolchain.sh
```

Individual components can be skipped with `--skip-sdk`, `--skip-gcc`, and
`--skip-sysconfig`.

The scripts do not modify the installed TI SDK.
