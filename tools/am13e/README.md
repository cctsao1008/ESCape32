# AM13E development tools

These scripts prepare and validate the Linux/WSL host environment used by the
`am13e-port` branch.

The versions are intentionally pinned to the versions expected by
AM13E230x SDK 26.01.00.03:

- AM13E230x SDK: `26_01_00_03`
- SysConfig: `1.28.0`
- Arm GNU Toolchain: `15.2.rel1`

Expected default layout:

```text
~/ti/am13e230x_sdk_26_01_00_03/
~/ti/sysconfig_1.28.0/
~/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/
```

## Check the environment

```bash
./tools/am13e/check-env.sh
```

The checker accepts the same environment overrides used by the TI SDK:

```bash
AM13E_SDK_ROOT=/path/to/sdk \
SYSCFG_PATH=/path/to/sysconfig \
GCC_ARM_TOOLCHAIN_PATH=/path/to/gcc \
./tools/am13e/check-env.sh
```

## Install the pinned toolchain

The installer can use already-downloaded archives/installers:

```bash
./tools/am13e/install-toolchain.sh \
  --gcc-archive /path/to/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz \
  --sysconfig-installer /path/to/sysconfig-1.28.0_4712-setup.run
```

If `--gcc-archive` is omitted, the script first searches common download
locations and then attempts to download the pinned Arm GNU archive from Arm's
official release location.

SysConfig is not downloaded automatically because TI's download endpoint can
change and may require interaction with the TI download page. If SysConfig is
not already installed, pass the Linux installer with
`--sysconfig-installer`.

Optional host packages can be installed with:

```bash
./tools/am13e/install-toolchain.sh --install-host-deps ...
```

The scripts do not modify the TI SDK.
