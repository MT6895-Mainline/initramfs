# Build initramfs for Redmi Note 11T Pro(+) / POCO X4 GT / Redmi K50i (xaga) Linux Mainline
## 1. Build `init` and package initramfs
```
make clean
make # BOOT_PARTITION=\"/dev/sdc86\" NVDATA_PARTITION=\"/dev/sdc13\"
# Default BOOT_PARTITION is /dev/sdc86. On xaga it's userdata.
# Default NVDATA_PARTITION is /dev/sdc13. On xaga it's nvdata. Init will mount and copy WIFI configuration from nvdata.
```
## 2. Done
```
ls -l root/init
ls -l initramfs.cpio.lz4
```
