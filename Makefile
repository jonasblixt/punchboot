# Makefile for Punch BOOT

ifndef BOARD
$(error BOARD is not defined)
endif

Q ?= @

# Default settings and apps
TARGET  = pb
PB_VERSION = $(shell cat version.txt)
BPAK ?= $(shell which bpak)
PYTHON ?= $(shell which python3)

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
SIZE=$(CROSS_COMPILE)size
STRIP=$(CROSS_COMPILE)strip
OBJCOPY=$(CROSS_COMPILE)objcopy

# Helper macros
unquote = $(subst $\",,$(1))

BUILD_DIR ?= build-$(lastword $(subst /, ,$(BOARD)))

KCONFIG_CONFIG ?= .config

BUILD_CONFIG ?= $(BUILD_DIR)/.config

# A build needs its configuration: a name that is mistyped would build the
# Kconfig defaults (F-229). menuconfig and olddefconfig make one, clean needs
# none.
ifneq ($(filter-out menuconfig olddefconfig clean,$(or $(MAKECMDGOALS),all)),)
ifeq ($(wildcard $(KCONFIG_CONFIG)),)
$(error KCONFIG_CONFIG=$(KCONFIG_CONFIG) doesn't exist: give a defconfig, or make one with menuconfig)
endif
endif

$(shell mkdir -p $(BUILD_DIR))
# A failure stops the build, which would otherwise use the old $(BUILD_CONFIG) (F-229)
ifneq ($(shell BOARD=$(BOARD) KCONFIG_CONFIG=$(KCONFIG_CONFIG) $(PYTHON) scripts/genconfig.py --header-path $(BUILD_DIR)/config.h --config-out $(BUILD_CONFIG) src/Kconfig >&2; echo $$?),0)
$(error scripts/genconfig.py failed)
endif

include $(BUILD_CONFIG)

# $(call value_file,<variable>) gives $(BUILD_DIR)/value/<variable>, a file
# with the variable's value that is rewritten only when the value changes. What
# is made from a variable depends on its file, so it is made again when the
# variable changes, in the configuration or on the command line (F-183).
value_file = $(shell f=$(BUILD_DIR)/value/$(1); v='$(subst ','\'',$($(1)))'; \
                 mkdir -p $(BUILD_DIR)/value; \
                 [ -f $$f ] && [ "$$(cat $$f)" = "$$v" ] || printf '%s\n' "$$v" > $$f; \
                 echo $$f)

cflags-y   = -std=c99
cflags-y  += -O$(call unquote,$(CONFIG_OPTIMIZE))
cflags-$(CONFIG_DEBUG_SYMBOLS) += -g
cflags-y  += -nostdlib -nostartfiles -nostdinc
cflags-y  += -ffunction-sections
cflags-y  += -fdata-sections
cflags-y  += -fno-omit-frame-pointer
cflags-y  += -DPB_VERSION=\"$(PB_VERSION)\"
cflags-y  += -DLOGLEVEL=$(CONFIG_LOGLEVEL)
cflags-y  += -fno-common -fno-builtin
cflags-y  += -ffreestanding -fno-exceptions
cflags-y  += -fstack-usage
cflags-y  += -fno-stack-protector
cflags-y  += -MMD -MP

# Include path
cflags-y  += -I src/
cflags-y  += -I include/
cflags-y  += -I include/libc
cflags-y  += -I $(BOARD)/include
cflags-y  += -I $(BUILD_DIR)

# Warnings
cflags-y += -Wall
cflags-y += -Wextra
cflags-y += -Wunused-result
#cflags-y += -Wmissing-include-dirs
cflags-y += -Wdisabled-optimization
cflags-y += -Wvla
cflags-y += -Wshadow
cflags-y += -Wno-unused-parameter
#cflags-y += -Wmissing-declarations
cflags-y += -Wmissing-format-attribute
cflags-y += -Wmissing-prototypes
cflags-y += -Wpointer-arith
cflags-y += -Wredundant-decls
cflags-y += -Waggregate-return

# Bootloader
src-y   = src/main.c
src-y  += keystore.c
src-y  += srk_fuse.c
src-y  += src/delay.c
src-y  += src/crypto.c
src-y  += src/bio.c
src-y  += src/device_uuid.c
src-y  += src/wire.c
src-y  += src/console.c
src-y  += src/log.c
src-y  += src/log_source.c
src-$(CONFIG_RAMLOG) += src/ramlog.c
src-y  += src/rot.c
src-y  += src/slc.c
src-y  += src/board.c

include src/drivers/*/makefile.mk
include src/boot/makefile.mk
include src/lib/makefile.mk
include $(BOARD)/makefile.mk
include src/arch/*/makefile.mk
include src/plat/*/makefile.mk
include src/cm/makefile.mk

ldflags-y += -Map=$(BUILD_DIR)/pb.map
ldflags-y += --defsym=PB_ENTRY=$(PB_ENTRY)
ldflags-y += --defsym=PB_STACK_SIZE_KiB=$(CONFIG_STACK_SIZE_KiB)
ldflags-y += -T$(CONFIG_LINK_FILE) --build-id=none
ldflags-y += --gc-sections

OBJS =
OBJS += $(patsubst %.c, $(BUILD_DIR)/%.o, $(src-y))
OBJS += $(patsubst %.S, $(BUILD_DIR)/%.o, $(asm-y))

BLOB_OBJS = $(patsubst %.bin, $(BUILD_DIR)/%.bino, $(blobs-y))
BLOB_OBJS2 = $(patsubst %.bin, $(BUILD_DIR)/%.bino, $(notdir $(blobs-y)))

DEPS      = $(OBJS:.o=.d)

FINAL_OUTPUT = $(BUILD_DIR)/$(TARGET).bin

.PHONY: menuconfig olddefconfig

menuconfig olddefconfig:
	$(Q)BOARD=$(BOARD) $(PYTHON) scripts/$@.py src/Kconfig

all: $(BUILD_DIR)/$(TARGET).bin $(plat-y)
	$(Q)$(SIZE) -x -t -B $(BUILD_DIR)/$(TARGET)
	@echo "Success, final output: $(FINAL_OUTPUT)"

# Every object can use PB_VERSION
$(OBJS): $(call value_file,PB_VERSION)

$(BUILD_DIR)/keystore.o: $(call unquote,$(CONFIG_KEYSTORE)) $(call value_file,CONFIG_KEYSTORE)
	@echo GEN $(BUILD_DIR)/keystore.c
	$(Q)$(BPAK) generate keystore --name pb $(CONFIG_KEYSTORE) --decorate > $(BUILD_DIR)/keystore.c
ifdef CONFIG_BOOT_FH
# pb's structure has room for 16 keys (boot.md 5.4). The number of keys is
# only in the generated keystore, where no static assert in C can read it
	$(Q)n=$$(sed -n 's/^ *\.no_of_keys = \([0-9][0-9]*\),.*/\1/p' $(BUILD_DIR)/keystore.c); \
	 if ! [ "$$n" -le 16 ] 2>/dev/null; then \
	     echo "CONFIG_BOOT_FH: the keystore has '$$n' keys, pb's structure holds 16 (boot.md 5.4)" >&2; \
	     exit 1; \
	 fi
endif
	$(Q)$(CC) -c $(cflags-y) $(BUILD_DIR)/keystore.c -o $(BUILD_DIR)/keystore.o

SRK_FUSE_FILE := $(patsubst "%",%,$(CONFIG_SRK_FUSE))

$(BUILD_DIR)/srk_fuse.o: $(SRK_FUSE_FILE) $(call value_file,CONFIG_SRK_FUSE)
	@echo GEN $(BUILD_DIR)/srk_fuse.c
	$(Q)$(PYTHON) scripts/gen-srk-fuse.py $(SRK_FUSE_FILE) > $(BUILD_DIR)/srk_fuse.c
	$(Q)$(CC) -c $(cflags-y) $(BUILD_DIR)/srk_fuse.c -o $@

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

.DEFAULT_GOAL := all

-include $(DEPS)
