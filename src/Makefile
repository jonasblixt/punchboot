# Makefile for Punch BOOT

TARGET  = pb
GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)
BPAK ?= $(shell which bpak)
KEYSTORE_BPAK ?= ../pki/internal_keystore.bpak

ifndef BOARD
	BOARD=test
endif

ifndef LOGLEVEL
	LOGLEVEL = 3
endif

ifndef BOARD_DIR
	BOARD_DIR=board
endif

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
CFLAGS  += -I. -I include/pb
CFLAGS  += -I include
CFLAGS  += -I include/pb/libc
CFLAGS  += -DPB_VERSION=\"$(GIT_VERSION)\"
CFLAGS  += -DLOGLEVEL=$(LOGLEVEL)
CFLAGS  += -DTIMING_REPORT=$(TIMING_REPORT)
CFLAGS  += -D__PB_BUILD
CFLAGS  += -fno-common -fno-builtin
CFLAGS  += -ffreestanding -fno-exceptions
CFLAGS  += -I 3pp/fdt/include
CFLAGS  += -I $(BOARD_DIR)/$(BOARD)/include
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

C_SRCS   = main.c
C_SRCS  += recovery.c
C_SRCS  += delay.c
C_SRCS  += config.c
C_SRCS  += crc.c
C_SRCS  += gpt.c
C_SRCS  += keystore.c
C_SRCS  += crypto.c
C_SRCS  += timing_report.c
C_SRCS  += usb.c
C_SRCS  += image.c
C_SRCS  += bpak.c
C_SRCS  += lib/string.c
C_SRCS  += lib/memmove.c
C_SRCS  += lib/memchr.c
C_SRCS  += lib/memcmp.c
C_SRCS  += lib/strcmp.c
C_SRCS  += lib/memset.c
C_SRCS  += lib/strlen.c
C_SRCS  += lib/printf.c
C_SRCS  += lib/snprintf.c
C_SRCS  += lib/putchar.c
C_SRCS  += lib/uuid.c

ifneq ($(BOARD),test)
C_SRCS  += boot.c
endif

# Lib fdt
C_SRCS  += 3pp/fdt/fdt.c
C_SRCS  += 3pp/fdt/fdt_addresses.c
C_SRCS  += 3pp/fdt/fdt_empty_tree.c
C_SRCS  += 3pp/fdt/fdt_ro.c
C_SRCS  += 3pp/fdt/fdt_rw.c
C_SRCS  += 3pp/fdt/fdt_strerror.c
C_SRCS  += 3pp/fdt/fdt_sw.c
C_SRCS  += 3pp/fdt/fdt_wip.c

LINT_EXCLUDE =
FINAL_IMAGE =
BUILD_DIR = build-$(BOARD)

$(shell mkdir -p $(BUILD_DIR))

include $(BOARD_DIR)/$(BOARD)/makefile.mk
include plat/$(PB_PLAT_NAME)/makefile.mk
include arch/$(PB_ARCH_NAME)/makefile.mk

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
STRIP=$(CROSS_COMPILE)strip
OBJCOPY=$(CROSS_COMPILE)objcopy

LDFLAGS += --defsym=PB_ENTRY=$(PB_ENTRY)
LDFLAGS += -Tarch/$(PB_ARCH_NAME)/link.lds
LDFLAGS += -Tplat/$(PB_PLAT_NAME)/link.lds
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

ifeq ($(BOARD),test)
CFLAGS += -g  -fprofile-arcs -ftest-coverage
include tests/makefile.mk
endif

.PHONY: plat_early keystore plat_final board_final

all: keystore plat_early $(BUILD_DIR)/$(TARGET).bin board_final plat_final
	$(info Summary:)
	$(info )
	$(info BOARD:     [${PB_BOARD_NAME}])
	$(info PLAT:      [${PB_PLAT_NAME}])
	$(info ARCH:      [${PB_ARCH_NAME}])
	$(info LOGLEVEL:  [${LOGLEVEL}])
	@echo "VERSION = $(GIT_VERSION)"
	$(info )
	@$(SIZE) -x -t -B $(BUILD_DIR)/$(TARGET)

keystore:
	@$(BPAK) generate keystore --name pb $(KEYSTORE_BPAK) > $(BUILD_DIR)/keystore.c
	@$(CC) -c $(CFLAGS) $(BUILD_DIR)/keystore.c -o $(BUILD_DIR)/keystore.o

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET)
	@echo OBJCOPY $< $@
	@cp $< $<_unstripped
	@$(STRIP) --strip-all $<
	@$(OBJCOPY) -O binary -R .comment $< $@

$(BUILD_DIR)/$(TARGET): $(OBJS) $(BLOB_OBJS)
	@echo LD $@
	@$(LD) $(LDFLAGS) $(OBJS) $(BLOB_OBJS) $(LIBS) -o $@

$(BUILD_DIR)/%.bino: %.bin
	@mkdir -p $(@D)
	@echo BLOB $<
	@$(OBJCOPY) -I binary -O $(ARCH_OUTPUT) -B $(ARCH) $< $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	@echo AS $<
	@$(CC) -D__ASSEMBLY__ -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

check:
	@cpplint --quiet $(shell git ls-files "*.[hc]")
pmccabe:
	@pmccabe  $(shell git ls-files "*.[hc]") | sort -n | tail -n 50
cloc:
	@cloc $$(git ls-files | grep -v 3pp | xargs)

clean:
	@-rm -rf $(BUILD_DIR)/

qemu:
	@$(QEMU) $(QEMU_FLAGS) $(QEMU_AUX_FLAGS) -kernel $(BUILD_DIR)/$(TARGET)

.DEFAULT_GOAL := all

-include $(DEPS)

