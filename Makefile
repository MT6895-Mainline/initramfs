# Build the minimal XAGA initramfs:
#   - compile init.c to root/init
#   - pack root/ into ../initramfs.cpio.lz4
#
# Usage:
#   make -C initramfs
#   make -C initramfs BOOT_PARTITION=/dev/sdc86
#   make -C initramfs clean

CROSS ?= aarch64-linux-gnu-
CC := $(CROSS)gcc
CPIO ?= cpio
LZ4 ?= lz4

BOOT_PARTITION ?= /dev/sdc86

ROOT := $(CURDIR)/root
INIT := $(ROOT)/init
OUT_LZ4 := $(CURDIR)/initramfs.cpio.lz4
TMP_CPIO := $(CURDIR)/initramfs.cpio

CFLAGS := -nostdlib -static -fno-stack-protector \
          -fno-builtin -ffreestanding -Os

.PHONY: all clean
all: $(OUT_LZ4)

$(INIT): init.c
	rm -f $(ROOT)/sys/init
	$(CC) $(CFLAGS) -DBOOT_PARTITION=\"$(BOOT_PARTITION)\" -o $@ init.c

$(TMP_CPIO): $(INIT)
	cd $(ROOT) && find . -print | $(CPIO) -o -H newc > $@

$(OUT_LZ4): $(TMP_CPIO)
	$(LZ4) -l -9 -f $< $@ >/dev/null

clean:
	rm -f $(INIT) $(ROOT)/sys/init $(TMP_CPIO) $(OUT_LZ4)
