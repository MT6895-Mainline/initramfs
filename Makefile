DEVICE ?= qqcandy
ifeq ($(filter $(DEVICE),qqcandy xaga),)
$(error DEVICE must be qqcandy or xaga)
endif
include profiles/$(DEVICE).mk

CROSS ?= aarch64-linux-gnu-
CC := $(CROSS)gcc
CPIO ?= cpio
LZ4 ?= lz4
FIRMWARE_DIR ?=
ROOT := $(CURDIR)/build/$(DEVICE)/root
INIT := $(ROOT)/init
OUT := $(CURDIR)/initramfs-$(DEVICE).cpio.lz4
CFLAGS := -nostdlib -static -no-pie -fno-stack-protector \
          -fno-builtin -ffreestanding -Os -Wall -Wextra -Werror

.PHONY: all clean FORCE
all: $(OUT)

# Config/partition changes must invalidate init even without source changes.
$(INIT): init.c profiles/$(DEVICE).mk FORCE
	rm -rf "$(ROOT)"
	mkdir -p "$(ROOT)/dev" "$(ROOT)/proc" "$(ROOT)/sys"
	$(CC) $(CFLAGS) $(DEVICE_CPPFLAGS) \
		-DBOOT_PARTITION=\"$(BOOT_PARTITION)\" \
		-DNVDATA_PARTITION=\"$(NVDATA_PARTITION)\" -o "$@" init.c

$(OUT): $(INIT)
	@if [ -n "$(FIRMWARE_DIR)" ]; then \
		test -d "$(FIRMWARE_DIR)" || exit 1; \
		test ! -e "$(FIRMWARE_DIR)/mediatek/mt6895/WIFI" || \
			{ echo "Refusing device-specific NVRAM in archive" >&2; exit 1; }; \
		test ! -e "$(FIRMWARE_DIR)/mediatek/mt6895/BT_Addr" || exit 1; \
		mkdir -p "$(ROOT)/lib/firmware"; \
		cp -a "$(FIRMWARE_DIR)/." "$(ROOT)/lib/firmware/"; \
	fi
	cd "$(ROOT)" && find . -print0 | LC_ALL=C sort -z | \
		$(CPIO) --null -o -H newc --owner=0:0 > "$(OUT:.lz4=)"
	$(LZ4) -l -9 -f "$(OUT:.lz4=)" "$@"

clean:
	rm -rf "$(CURDIR)/build/$(DEVICE)"
	rm -f "$(OUT)" "$(OUT:.lz4=)"
