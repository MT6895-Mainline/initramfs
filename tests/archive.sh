#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for device in qqcandy xaga; do
	make DEVICE="$device"
	list="$(lz4 -dc "initramfs-$device.cpio.lz4" | cpio -it 2>/dev/null)"
	grep -qx init <<< "$list"
	! grep -q firmware <<< "$list"
	readelf -h "build/$device/root/init" | grep -q 'Machine:.*AArch64'
	! readelf -l "build/$device/root/init" | grep -q INTERP
done
strings build/qqcandy/root/init | grep -qx /dev/sdc80
strings build/qqcandy/root/init | grep -qx /dev/sdc10
strings build/qqcandy/root/init | grep -qx oplus,qqcandy
! strings build/qqcandy/root/init | grep -q novatek_nt36672e
echo "Both source-only archives and qqcandy board/partition guards validated"
