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

$(shell BOARD=$(BOARD) $(PYTHON) scripts/genconfig.py)
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
cflags-y  += -nostdlib -nostartfiles
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
cflags-y  += -I lib/fdt/include
cflags-y  += -I lib/uuid/
cflags-y  += -I. -I include/ -I lib/
cflags-y  += -I include
cflags-y  += -I include/pb/libc
cflags-y  += -I $(BOARD)/include

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
src-y   = main.c
src-y  += delay.c
src-y  += boot.c
src-$(CONFIG_BOOT_AB)  += ab.c
src-y  += keystore.c
src-y  += keystore_helpers.c
src-y  += timestamp.c
src-y  += usb.c
src-y  += image.c
src-y  += storage.c
src-y  += wire.c
src-y  += command.c


BUILD_DIR ?= build-$(lastword $(subst /, ,$(BOARD)))

include lib/makefile.mk
include $(BOARD)/makefile.mk
include arch/*/makefile.mk
include plat/*/makefile.mk

$(shell mkdir -p $(BUILD_DIR))

ldflags-y += -Map=$(BUILD_DIR)/pb.map 
ldflags-y += --defsym=PB_ENTRY=$(PB_ENTRY)
ldflags-y += --defsym=PB_STACK_SIZE_KB=$(CONFIG_STACK_SIZE_KB)
ldflags-y += -Tlink.lds  --build-id=none

OBJS =
OBJS += $(patsubst %.c, $(BUILD_DIR)/%.o, $(src-y))
OBJS += $(patsubst %.S, $(BUILD_DIR)/%.o, $(asm-y))

BLOB_OBJS = $(patsubst %.bin, $(BUILD_DIR)/%.bino, $(blobs-y))

DEPS      = $(OBJS:.o=.d)

FINAL_OUTPUT = $(BUILD_DIR)/$(TARGET).bin

.PHONY: keystore menuconfig

menuconfig:
	$(Q)BOARD=$(BOARD) $(PYTHON) scripts/menuconfig.py 

all: $(BUILD_DIR)/$(TARGET).bin $(plat-y)
	$(Q)$(SIZE) -x -t -B $(BUILD_DIR)/$(TARGET)
	@echo "Success, final output: $(FINAL_OUTPUT)"

keystore:
	$(Q)$(BPAK) generate keystore --name pb $(CONFIG_KEYSTORE) > $(BUILD_DIR)/keystore.c
	$(Q)$(CC) -c $(cflags-y) $(BUILD_DIR)/keystore.c -o $(BUILD_DIR)/keystore.o

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET)
	@echo OBJCOPY $< $@
	$(Q)cp $< $<_unstripped
	$(Q)$(STRIP) --strip-all $<
	$(Q)$(OBJCOPY) -O binary -R .comment $< $@

$(BUILD_DIR)/$(TARGET): keystore $(OBJS) $(BLOB_OBJS)
	@echo LD $@
	$(Q)$(LD) $(ldflags-y) $(OBJS) $(BLOB_OBJS) $(LIBS) -o $@

$(BUILD_DIR)/%.bino: %.bin
	@mkdir -p $(@D)
	@echo BLOB $< $@
	$(Q)$(OBJCOPY) -I binary -O $(ARCH_OUTPUT) -B $(ARCH) $< $@

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
	@gcovr 	--gcov-exclude plat \
			--gcov-exclude uuid \
			--gcov-exclude fdt \
			--gcov-exclude tests \
			--gcov-exclude lib \
			--gcov-exclude bearssl

.DEFAULT_GOAL := all

-include $(DEPS)

