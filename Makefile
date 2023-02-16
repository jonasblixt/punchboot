# Makefile for Punch BOOT

ifndef BOARD
$(error BOARD is not defined)
endif

Q ?= @

TARGET  = pb
GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)
BPAK ?= $(shell which bpak)
KEYSTORE_BPAK ?= pki/internal_keystore.bpak
PYTHON ?= $(shell which python3)

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
STRIP=$(CROSS_COMPILE)strip
OBJCOPY=$(CROSS_COMPILE)objcopy

BUILD_DIR ?= build-$(lastword $(subst /, ,$(BOARD)))
$(shell mkdir -p $(BUILD_DIR))
$(shell BOARD=$(BOARD) $(PYTHON) scripts/genconfig.py --header-path $(BUILD_DIR)/config.h src/Kconfig)

include .config

ifdef TIMING_REPORT
	LOGLEVEL = 0
	TIMING_REPORT=1
else
	TIMING_REPORT=0
endif

cflags-y   = -std=c99
cflags-y  += -O$(CONFIG_OPTIMIZE)
cflags-$(CONFIG_DEBUG_SYMBOLS) += -g
cflags-y  += -nostdlib -nostartfiles -nostdinc
cflags-y  += -ffunction-sections
cflags-y  += -fdata-sections
cflags-y  += -fno-omit-frame-pointer
cflags-y  += -DPB_VERSION=\"$(GIT_VERSION)\"
cflags-y  += -DLOGLEVEL=$(CONFIG_LOGLEVEL)
cflags-y  += -DTIMING_REPORT=$(TIMING_REPORT)
cflags-y  += -fno-common -fno-builtin
cflags-y  += -ffreestanding -fno-exceptions
cflags-y  += -fstack-usage
cflags-y  += -MMD -MP

# Include path
cflags-y  += -I src/ -I src/include/
cflags-y  += -I src/include
cflags-y  += -I src/libc/include
cflags-y  += -I $(BOARD)/include
cflags-y  += -I $(BUILD_DIR)

# Warnings
cflags-y += -Wall
cflags-y += -Wextra
cflags-y += -Wunused-result
#cflags-y += -Wmissing-include-dirs
cflags-y += -Wunused
cflags-y += -Wdisabled-optimization
cflags-y += -Wvla
cflags-y += -Wshadow
cflags-y += -Wno-unused-parameter
cflags-y += -Wextra
#cflags-y += -Wmissing-declarations
cflags-y += -Wmissing-format-attribute
cflags-y += -Wmissing-prototypes
cflags-y += -Wpointer-arith
cflags-y += -Wredundant-decls
cflags-y += -Waggregate-return

# Bootloader
src-y   = src/main.c
src-y  += src/boot.c
src-$(CONFIG_BOOT_AB)  += src/ab.c
src-y  += keystore.c
src-y  += src/time.c
src-y  += src/usb.c
src-y  += src/image.c
src-y  += src/storage.c
src-y  += src/wire.c
src-y  += src/command.c
src-y  += src/gpt.c
src-y  += src/fletcher.c
src-y  += src/bpak.c
src-y  += src/crc.c
src-y  += src/asn1.c

# UUID lib
src-y  += src/uuid/pack.c
src-y  += src/uuid/unpack.c
src-y  += src/uuid/compare.c
src-y  += src/uuid/copy.c
src-y  += src/uuid/unparse.c
src-y  += src/uuid/parse.c
src-y  += src/uuid/clear.c
src-y  += src/uuid/conv.c
src-$(CONFIG_LIB_UUID3)  += src/uuid/uuid3.c
cflags-y  += -I src/uuid/include

# Device tree lib
src-y  += src/fdt/fdt.c
src-y  += src/fdt/fdt_addresses.c
src-y  += src/fdt/fdt_ro.c
src-y  += src/fdt/fdt_rw.c
src-y  += src/fdt/fdt_sw.c
src-y  += src/fdt/fdt_wip.c
cflags-y  += -I src/fdt/include

# VM/MMU helpers
src-y += src/vm/xlat_tables_common.c
cflags-y += -I src/vm/include
src-$(CONFIG_ARCH_ARMV7) += src/vm/aarch32/xlat_tables.c
src-$(CONFIG_ARCH_ARMV8) += src/vm/aarch64/xlat_tables.c
cflags-$(CONFIG_ARCH_ARMV7) += -I src/vm/include/vm/aarch32
cflags-$(CONFIG_ARCH_ARMV8) += -I src/vm/include/vm/aarch64

include src/bearssl/makefile.mk
include src/libc/makefile.mk
include $(BOARD)/makefile.mk
include src/arch/*/makefile.mk
include src/plat/*/makefile.mk

ldflags-y += -Map=$(BUILD_DIR)/pb.map
ldflags-y += --defsym=PB_ENTRY=$(PB_ENTRY)
ldflags-y += --defsym=PB_STACK_SIZE_KB=$(CONFIG_STACK_SIZE_KB)
ldflags-y += -Tsrc/link.lds  --build-id=none

OBJS =
OBJS += $(patsubst %.c, $(BUILD_DIR)/%.o, $(src-y))
OBJS += $(patsubst %.S, $(BUILD_DIR)/%.o, $(asm-y))

BLOB_OBJS = $(patsubst %.bin, $(BUILD_DIR)/%.bino, $(blobs-y))
BLOB_OBJS2 = $(patsubst %.bin, $(BUILD_DIR)/%.bino, $(notdir $(blobs-y)))

DEPS      = $(OBJS:.o=.d)

FINAL_OUTPUT = $(BUILD_DIR)/$(TARGET).bin

.PHONY: keystore menuconfig

menuconfig:
	$(Q)BOARD=$(BOARD) $(PYTHON) scripts/menuconfig.py src/Kconfig

all: $(BUILD_DIR)/$(TARGET).bin $(plat-y)
	$(Q)$(SIZE) -x -t -B $(BUILD_DIR)/$(TARGET)
	@echo "Success, final output: $(FINAL_OUTPUT)"

$(BUILD_DIR)/keystore.o:
	@echo GEN $(BUILD_DIR)/keystore.c
	$(Q)$(BPAK) generate keystore --name pb $(CONFIG_KEYSTORE) --decorate > $(BUILD_DIR)/keystore.c
	$(Q)$(CC) -c $(cflags-y) $(BUILD_DIR)/keystore.c -o $(BUILD_DIR)/keystore.o

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET)
	@echo OBJCOPY $< $@
	$(Q)cp $< $<_unstripped
	$(Q)$(STRIP) --strip-all $<
	$(Q)$(OBJCOPY) -O binary -R .comment $< $@

$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/keystore.o $(OBJS) $(BLOB_OBJS)
	@echo LD $@
	$(Q)$(LD) $(ldflags-y) $(OBJS) $(BLOB_OBJS2) $(LIBS) -o $@

$(BUILD_DIR)/%.bino: %.bin
	@mkdir -p $(@D)
	@mkdir -p blobs/
	@mkdir -p $(BUILD_DIR)/blobs
	@cp $< blobs/
	@echo BLOB $< $(notdir $<)
	$(Q)$(OBJCOPY) -I binary -O $(ARCH_OUTPUT) -B $(ARCH) blobs/$(notdir $<) $(BUILD_DIR)/$(notdir $@)

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	@echo AS $<
	$(Q)$(CC) -D__ASSEMBLY__ -c $(cflags-y) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo CC $<
	$(Q)$(CC) -c $(cflags-y) $< -o $@

clean:
	@-rm -rf $(BUILD_DIR)/
	@-rm -rf *.gcov

install: all
	@test -d $(INSTALL_DIR) || echo "You must set INSTALL_DIR"
	@test -d $(INSTALL_DIR)
	@echo Installing $(FINAL_OUTPUT) to '$(INSTALL_DIR)'
	@cp $(FINAL_OUTPUT) $(INSTALL_DIR)

gcovr:
	@gcovr 	--gcov-exclude src/plat \
			--gcov-exclude uuid \
			--gcov-exclude src/fdt \
			--gcov-exclude tests \
			--gcov-exclude src/lib \
			--gcov-exclude bearssl

.DEFAULT_GOAL := all

-include $(DEPS)

