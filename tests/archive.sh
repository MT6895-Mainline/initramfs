#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for device in qqcandy xaga; do
	make DEVICE="$device"
	list="$(lz4 -dc "initramfs-$device.cpio.lz4" | cpio -it 2>/dev/null)"
	grep -qx init <<< "$list"
	if [ "$device" = qqcandy ]; then
		grep -qx xinit <<< "$list"
		test "build/$device/root/init" -ef "build/$device/root/xinit"
	else
		! grep -qx xinit <<< "$list"
	fi
	! grep -q firmware <<< "$list"
	readelf -h "build/$device/root/init" | grep -q 'Machine:.*AArch64'
	! readelf -l "build/$device/root/init" | grep -q INTERP
done
strings build/qqcandy/root/init | grep -qx /dev/sdc80
strings build/qqcandy/root/init | grep -qx /dev/sdc10
strings build/qqcandy/root/init | grep -qx oplus,qqcandy
strings build/qqcandy/root/init | grep -qx /sys/bus/platform/devices/112b0000.ufshci/power/control
! strings build/xaga/root/init | grep -q 112b0000.ufshci
! strings build/qqcandy/root/init | grep -q novatek_nt36672e
fixture="$(mktemp -d)"
trap 'rm -rf "$fixture"' EXIT
mkdir -p "$fixture/mediatek/mt6895"
printf 'archive isolation fixture\n' > "$fixture/test-firmware.bin"
make DEVICE=qqcandy FIRMWARE_DIR="$fixture"
lz4 -dc initramfs-qqcandy.cpio.lz4 | cpio -it 2>/dev/null | grep -q test-firmware.bin
make DEVICE=qqcandy
! lz4 -dc initramfs-qqcandy.cpio.lz4 | cpio -it 2>/dev/null | grep -q firmware
for private in WIFI BT_Addr; do
	touch "$fixture/mediatek/mt6895/$private"
	if make DEVICE=qqcandy FIRMWARE_DIR="$fixture" > "$fixture/rejection.log" 2>&1; then
		echo "Private calibration was accepted: $private" >&2
		exit 1
	fi
	rm "$fixture/mediatek/mt6895/$private"
done
make DEVICE=qqcandy
echo "Both source-only archives and qqcandy board/partition guards validated"
echo "Private calibration rejected; default archive clears previous BYO firmware"
