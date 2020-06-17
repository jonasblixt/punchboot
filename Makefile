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

$(shell BOARD=$(BOARD) $(PYTHON) scripts/genconfig.py)
include .config

ifdef TIMING_REPORT
	LOGLEVEL = 0
	TIMING_REPORT=1
else
	TIMING_REPORT=0
endif

CFLAGS   = -Wall -Wextra -Wunused-result -Os -std=c99
CFLAGS  += -nostdlib -nostartfiles
CFLAGS  += -ffunction-sections
CFLAGS  += -fdata-sections
CFLAGS  += -fno-omit-frame-pointer
CFLAGS  += -I. -I include/
CFLAGS  += -I include
CFLAGS  += -I include/pb/libc
CFLAGS  += -DPB_VERSION=\"$(GIT_VERSION)\"
CFLAGS  += -DLOGLEVEL=$(CONFIG_LOGLEVEL)
CFLAGS  += -DTIMING_REPORT=$(TIMING_REPORT)
CFLAGS  += -D__PB_BUILD
CFLAGS  += -fno-common -fno-builtin
CFLAGS  += -ffreestanding -fno-exceptions
CFLAGS  += -I fdt/include
CFLAGS  += -I uuid/
CFLAGS  += -fstack-usage
CFLAGS  += -MMD -MP

# General warnings
WARNING = -Wall -Wmissing-include-dirs -Wunused \
			-Wdisabled-optimization	-Wvla -Wshadow \
			-Wno-unused-parameter
WARNING += -Wextra
#WARNING += -Wmissing-declarations
WARNING += -Wmissing-format-attribute
#WARNING += -Wmissing-prototypes
WARNING += -Wpointer-arith
WARNING += -Wredundant-decls
WARNING += -Waggregate-return

CFLAGS  += $(WARNING)

LIBS     =

LDFLAGS =

ASM_SRCS =
ARCH_ASM_SRCS =
ARCH_C_SRCS =
PLAT_ASM_SRCS =
PLAT_C_SRCS =
BLOB_INPUT  =

# Bootloader
C_SRCS   = main.c
C_SRCS  += delay.c
C_SRCS  += boot.c
ifdef CONFIG_BOOT_AB
C_SRCS  += ab.c
endif
C_SRCS  += crc.c
C_SRCS  += gpt.c
C_SRCS  += keystore.c
C_SRCS  += keystore_helpers.c
C_SRCS  += asn1.c
C_SRCS  += timestamp.c
C_SRCS  += usb.c
C_SRCS  += image.c
C_SRCS  += bpak.c
C_SRCS  += storage.c
C_SRCS  += wire.c
C_SRCS  += command.c

# UUID lib
C_SRCS  += uuid/pack.c
C_SRCS  += uuid/unpack.c
C_SRCS  += uuid/compare.c
C_SRCS  += uuid/copy.c
C_SRCS  += uuid/unparse.c
C_SRCS  += uuid/parse.c
C_SRCS  += uuid/clear.c
C_SRCS  += uuid/conv.c
C_SRCS  += uuid/uuid3.c

# C library
C_SRCS  += lib/string.c
C_SRCS  += lib/memmove.c
C_SRCS  += lib/memchr.c
C_SRCS  += lib/memcmp.c
C_SRCS  += lib/strcmp.c
C_SRCS  += lib/memset.c
C_SRCS  += lib/strlen.c
C_SRCS  += lib/printf.c
C_SRCS  += lib/snprintf.c
C_SRCS  += lib/strtoul.c
C_SRCS  += lib/putchar.c
C_SRCS  += lib/fletcher.c

# Lib fdt
C_SRCS  += fdt/fdt.c
C_SRCS  += fdt/fdt_addresses.c
C_SRCS  += fdt/fdt_ro.c
C_SRCS  += fdt/fdt_rw.c
C_SRCS  += fdt/fdt_sw.c
C_SRCS  += fdt/fdt_wip.c

BUILD_DIR ?= build-$(lastword $(subst /, ,$(BOARD)))

include $(BOARD)/makefile.mk
include arch/*/makefile.mk
include plat/*/makefile.mk
include bearssl/makefile.mk
#include tests/makefile.mk

$(shell mkdir -p $(BUILD_DIR))

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
STRIP=$(CROSS_COMPILE)strip
OBJCOPY=$(CROSS_COMPILE)objcopy

LDFLAGS += --defsym=PB_ENTRY=$(PB_ENTRY)
LDFLAGS += -Tlink.lds  --build-id=none

ARCH_OBJS     = $(patsubst %.S, $(BUILD_DIR)/%.o, $(ARCH_ASM_SRCS))
ARCH_OBJS    += $(patsubst %.c, $(BUILD_DIR)/%.o, $(ARCH_C_SRCS))
PLAT_OBJS     = $(patsubst %.S, $(BUILD_DIR)/%.o, $(PLAT_ASM_SRCS))
PLAT_OBJS    += $(patsubst %.c, $(BUILD_DIR)/%.o, $(PLAT_C_SRCS))
BOARD_OBJS    = $(patsubst %.S, $(BUILD_DIR)/%.o, $(BOARD_ASM_SRCS))
BOARD_OBJS   += $(patsubst %.c, $(BUILD_DIR)/%.o, $(BOARD_C_SRCS))

OBJS	  = $(ARCH_OBJS) $(PLAT_OBJS) $(BOARD_OBJS)
OBJS     += $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SRCS))
OBJS     += $(patsubst %.S, $(BUILD_DIR)/%.o, $(ASM_SRCS))

BLOB_OBJS = $(patsubst %.bin, $(BUILD_DIR)/%.bino, $(BLOB_INPUT))

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
	$(Q)$(CC) -c $(CFLAGS) $(BUILD_DIR)/keystore.c -o $(BUILD_DIR)/keystore.o

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET)
	@echo OBJCOPY $< $@
	$(Q)cp $< $<_unstripped
	$(Q)$(STRIP) --strip-all $<
	$(Q)$(OBJCOPY) -O binary -R .comment $< $@

$(BUILD_DIR)/$(TARGET): keystore $(OBJS) $(BLOB_OBJS)
	@echo LD $@
	$(Q)$(LD) $(LDFLAGS) $(OBJS) $(BLOB_OBJS) $(LIBS) -o $@

$(BUILD_DIR)/%.bino: %.bin
	@mkdir -p $(@D)
	@echo BLOB $< $@
	$(Q)$(OBJCOPY) -I binary -O $(ARCH_OUTPUT) -B $(ARCH) $< $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	@echo AS $<
	$(Q)$(CC) -D__ASSEMBLY__ -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo CC $<
	$(Q)$(CC) -c $(CFLAGS) $< -o $@

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

