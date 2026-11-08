# The MCU test bench's application (README.md). Builds one signed boot
# image per mode in build/<board>/:
#
#   make BOARD=imxrt1060evk         (in the dev container: scripts/pbdev run
#                                    make -C testbench/mcu)
#
# Needs arm-none-eabi-gcc and the bpak CLI, both in the dev container.

BOARD ?= imxrt1060evk
MODES ?= good noverify hang boottime
CROSS_COMPILE ?= arm-none-eabi-
BPAK ?= bpak
# Holding time before the reset in the 'good' and 'noverify' modes
APP_HOLD_MS ?= 500
# Warm resets that the 'boottime' mode times
APP_BOOT_RUNS ?= 10

TOP := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
PB := $(abspath $(TOP)/../..)
BUILD := $(TOP)/build/$(BOARD)

include $(TOP)/board/$(BOARD)/board.mk

CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE := $(CROSS_COMPILE)size

CFLAGS := $(CPU_FLAGS) -Os -g -std=c11 -ffreestanding -nostdlib -fno-builtin \
	-ffunction-sections -fdata-sections -Wall -Wextra -Werror \
	-I$(TOP)/src -I$(PB)/include \
	-DAPP_BOARD='"$(BOARD)"' -DAPP_NUM_IRQS=$(APP_NUM_IRQS) \
	-DAPP_LOAD_ADDR=$(APP_LOAD_ADDR) -DAPP_HOLD_MS=$(APP_HOLD_MS) \
	-DAPP_BOOT_RUNS=$(APP_BOOT_RUNS)
LDFLAGS := $(CPU_FLAGS) -nostdlib -Wl,--gc-sections -T$(TOP)/board/$(BOARD)/link.ld -lgcc

SRCS := $(TOP)/src/start.S $(TOP)/src/main.c $(TOP)/src/pbstate.c \
	$(TOP)/board/$(BOARD)/board.c $(PB)/src/lib/crc.c $(BOARD_SRCS)

mode_flags = -DAPP_MODE_NAME='"$(1)"' -DAPP_MODE_GOOD=$(if $(filter good,$(1)),1,0) \
	-DAPP_MODE_NOVERIFY=$(if $(filter noverify,$(1)),1,0) \
	-DAPP_MODE_HANG=$(if $(filter hang,$(1)),1,0) \
	-DAPP_MODE_BOOTTIME=$(if $(filter boottime,$(1)),1,0) \
	-DAPP_KICK_WDOG=$(if $(filter hang,$(1)),0,1)

all: $(foreach m,$(MODES),$(BUILD)/app-$(m).bpak)

$(BUILD)/app-%.elf: $(SRCS) $(TOP)/src/app.h $(TOP)/board/$(BOARD)/link.ld $(TOP)/board/$(BOARD)/board.mk
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(call mode_flags,$*) $(SRCS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/app-%.bin: $(BUILD)/app-%.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/app-%.bpak: $(BUILD)/app-%.bin $(TOP)/scripts/mkimage
	BPAK=$(BPAK) $(TOP)/scripts/mkimage $< $(APP_LOAD_ADDR) $@

clean:
	rm -rf $(TOP)/build

.PHONY: all clean
.SECONDARY:
