# Build initramfs for Redmi Note 11T Pro(+) / POCO X4 GT / Redmi K50i (xaga) Linux Mainline
## 1. Readback `nvdata` partition
```
# Avoid using "dd if=/dev/block/by-name/nvdata of=/sdcard/nvdata.img", or readback image will be corrupted.
# Use SP Flash Tool V6 instead.
```
## 2. Mount `nvdata`
```
mkdir tmp-mount-nvdata
sudo mount nvdata.img tmp-mount-nvdata
```
## 3. Extract WIFI configuration
```
cp -r tmp-mount-nvdata/APCFG/APRDEB/WIFI root/lib/firmware/mediatek/mt6895/WIFI
```
## 4. Build `init` and package initramfs
```
make clean
make
```
## 5. Done
```
ls -l root/lib/firmware/mediatek/mt6895/WIFI
ls -l root/init
ls -l initramfs.cpio.lz4
```