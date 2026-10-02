# qqcandy initramfs

This branch supports OPPO K10 / OnePlus Ace Racing Edition (21143/22801).
It does not assume Xaga partition numbers or touchscreen firmware.

```sh
make DEVICE=qqcandy
make DEVICE=qqcandy FIRMWARE_DIR=/path/to/your/firmware
make DEVICE=xaga                          # legacy explicit profile
```

Outputs are `initramfs-<device>.cpio.lz4`; staging is isolated in
`build/<device>/root`. No tracked Xaga firmware is packed implicitly.
An optional external firmware directory must contain only files appropriate
for the selected board. Never include WIFI/BT_Addr device calibration in
published archives. No automatic download or flashing is performed.

Measured qqcandy nodes are userdata=/dev/sdc80 and nvdata=/dev/sdc10. The
qqcandy init verifies the board compatible and PARTNAME in sysfs before
mounting; a mismatch stops boot rather than using another partition.
Nvdata is mounted ext4 read-only with `noload` (no journal replay). Calibration
is copied only if a rootfs file is absent. No modem NV partition is modified.

The firmware mirror list comes from the board's working kernel firmware
configuration, not the Xaga touch panel. QQcandy uses NT36672C Tianma, not
NT36672E L16. This is a compile/archive-tested candidate, not a flashed or
boot-tested artifact. Keep a known-good boot image and a board-specific
recovery procedure before any deployment. The usual boot watchdog delay
is not evidence of a boot loop.
