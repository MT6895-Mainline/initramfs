# Build initramfs for MT6895 Linux Mainline

The `qqcandy-mt6895` branch adds OPlus qqcandy (PGZ110, mixed 21143/22801)
alongside Redmi Note 11T Pro(+) / POCO X4 GT / Redmi K50i (xaga).
This repository owns the early boot chain, not the distribution rootfs or UCM.

| Profile | Default rootfs partition | Default nvdata partition |
| --- | --- | --- |
| `qqcandy` (default) | `/dev/sdc80` (userdata) | `/dev/sdc10` |
| `xaga` | `/dev/sdc86` (userdata) | `/dev/sdc13` |

These numbers are board-specific, not shared MT6895 defaults. qqcandy verifies
the DT compatible and userdata/nvdata PARTNAME before using them; a mismatch
does not fall back to another partition. Its touchscreen firmware name is
`FW_NF_NT36672C_TIANMA.img`, not Xaga's NT36672E firmware.

## 1. Build `init` and package initramfs
Requires an AArch64 GCC cross compiler, Make, cpio and lz4. `init` is a static,
freestanding ARM64 executable, with no dynamic loader or runtime libc dependency.

```sh
make DEVICE=qqcandy
make DEVICE=xaga
# Optional: CROSS=aarch64-linux-gnu- BOOT_PARTITION=/dev/... NVDATA_PARTITION=/dev/...
```

Each profile gets a fresh `build/<device>/root` and separate archive. By default
the archive contains source-built init only, not proprietary firmware or private
calibration. A private local build may explicitly set `FIRMWARE_DIR=/path/to/blobs`;
the known WIFI/BT_Addr calibration paths are rejected. Do not publish private
firmware archives without checking licenses and all device-specific contents.

At boot, nvdata is mounted read-only with ext4 journal replay disabled (`noload`).
WiFi/BT calibration may be copied to the rootfs, without overwriting an existing
file. This does not write nvdata. Required firmware/kernel modules still need
separate board-specific provisioning.

## 2. Done
```sh
ls -l build/qqcandy/root/init initramfs-qqcandy.cpio.lz4
make DEVICE=qqcandy clean
```

## Validation and Integration

```sh
bash tests/archive.sh
```

Offline tests pass for both source-only archives, ARM64/static ELF, qqcandy
profile strings, private calibration rejection, and removal of stale firmware
between builds. These tests do not prove device boot or test live NV access.
This branch has not been flashed as part of the rootfs integration work.

Pair this with a matching [kernel](https://github.com/MT6895-Mainline/linux),
[rootfs](https://github.com/MT6895-Mainline/rootfs) and board-specific
[quirks](https://github.com/MT6895-Mainline/quirks/tree/qqcandy-mt6895).
Packing/flashing boot.img is outside this builder; validate the complete boot
chain and retain verified backups before deployment.
