# Measured on qqcandy: /sys/class/block/{sdc80,sdc10}/uevent.
# Both 21143 and 22801 are the supported mixed board family.
BOOT_PARTITION ?= /dev/sdc80
NVDATA_PARTITION ?= /dev/sdc10
DEVICE_CPPFLAGS := -DDEVICE_QQCANDY
