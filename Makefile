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
src-y  += src/dma.c
src-y  += src/crypto.c
src-y  += src/bio.c
src-y  += src/device_uuid.c
src-y  += src/wire.c
src-y  += src/console.c
src-y  += src/log.c
src-y  += src/log_source.c
src-y  += src/plat_info.c
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

# The board's memory (platform.md 5.1): where pb is linked, pb's region, the
# board's DRAM and pb's buffers. They are facts of the board's hardware, so
# its makefile.mk sets them, and the platform's makefile.mk, read after it,
# gives the buffers a default with ?=. Each reaches the linker (--defsym) and
# C (-D) under its own name. A value that is missing would otherwise fail in
# the linker, without its name.
PB_SIZE ?= 0x0
BOARD_MEMORY = PB_ENTRY PB_SIZE BOARD_DRAM_BASE BOARD_DRAM_SIZE PB_BUFFERS_BASE PB_BUFFERS_SIZE
ifneq ($(filter-out menuconfig olddefconfig clean,$(or $(MAKECMDGOALS),all)),)
$(foreach v,$(BOARD_MEMORY),$(if $(strip $($(v))),,\
    $(error $(v) is not set: the board's makefile.mk gives its memory (platform.md 5.1))))
endif

# Before 3.0.0 the board's makefile.mk gave the DRAM as -D flags: they would
# be defined twice, with the board's values or with these
ifneq ($(filter -DBOARD_RAM_BASE=% -DBOARD_RAM_END=%,$(cflags-y)),)
$(error -DBOARD_RAM_BASE and -DBOARD_RAM_END are BOARD_DRAM_BASE and BOARD_DRAM_SIZE in the board's makefile.mk since 3.0.0 (platform.md 5.1))
endif

cflags-y  += $(foreach v,$(BOARD_MEMORY),-D$(v)=$($(v)))

# The keys (board.md 7, D-083): VERIFY_KEY<n>_PATH and VERIFY_KEY<n>_ID in the
# board's makefile.mk or on the command line, n the key's revoke slot. A board
# that names no key gets pb's development keys, all of them: one that names a
# key gets only its own, never a test key in a slot it left empty.
KEY_VARIABLES = $(filter VERIFY_KEY%_PATH VERIFY_KEY%_ID,$(.VARIABLES))
ifeq ($(KEY_VARIABLES),)
include pki/dev-keys.mk
DEV_KEYS = 1
endif
KEY_SLOTS = 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
KEY_PATHS = $(foreach n,$(KEY_SLOTS),$(VERIFY_KEY$(n)_PATH))
KEYSTORE_ARGS = $(foreach n,$(KEY_SLOTS),$(if $(VERIFY_KEY$(n)_PATH),$(n) $(VERIFY_KEY$(n)_ID) $(VERIFY_KEY$(n)_PATH)))
ifneq ($(filter-out menuconfig olddefconfig clean,$(or $(MAKECMDGOALS),all)),)
$(foreach v,$(filter-out $(foreach n,$(KEY_SLOTS),VERIFY_KEY$(n)_PATH VERIFY_KEY$(n)_ID),$(KEY_VARIABLES)),\
    $(error $(v): not a key, the slots are 0 to 15 (board.md 7)))
$(foreach n,$(KEY_SLOTS),$(if $(and $(strip $(VERIFY_KEY$(n)_PATH)),$(strip $(VERIFY_KEY$(n)_ID))),,\
    $(if $(strip $(VERIFY_KEY$(n)_PATH)$(VERIFY_KEY$(n)_ID)),\
        $(error VERIFY_KEY$(n): a key has both VERIFY_KEY$(n)_PATH and VERIFY_KEY$(n)_ID (board.md 7)))))
$(if $(strip $(KEYSTORE_ARGS)),,$(error VERIFY_KEY<n>_PATH: the board names no key (board.md 7)))
# Before 3.0.0 the keys were a bpak keystore in the configuration. A 2.x
# defconfig would otherwise build with the development keys.
ifneq ($(or $(filter-out undefined,$(origin CONFIG_KEYSTORE) $(origin CONFIG_KEYSTORE_SLOTS)),\
            $(shell grep -E '^CONFIG_KEYSTORE(_SLOTS)?=' $(KCONFIG_CONFIG))),)
$(error CONFIG_KEYSTORE and CONFIG_KEYSTORE_SLOTS are the board's VERIFY_KEY<n>_PATH and VERIFY_KEY<n>_ID since 3.0.0 (board.md 7, migration-3.0.md 13))
endif
# Before 3.0.0 CONFIG_EXECUTE_IN_FLASH chose src/link_flash.lds, for a flash
# region no platform has, and the ARMv7-M copied .data from the flash. A
# configuration with it would otherwise link for RAM, without the copy.
ifneq ($(shell grep -E '^CONFIG_EXECUTE_IN_FLASH=y' $(KCONFIG_CONFIG)),)
$(error CONFIG_EXECUTE_IN_FLASH: pb runs from RAM since 3.0.0, no platform has a flash region (F-428))
endif
endif

# BOARD_RAM_BASE and BOARD_RAM_END are the DRAM's names in the platforms'
# memory maps; the end is made here, where it doesn't wrap in 32 bits.
BOARD_DRAM_END := $(shell printf '0x%x' $$(( $(BOARD_DRAM_BASE) + $(BOARD_DRAM_SIZE) )))
cflags-y  += -DBOARD_RAM_BASE=$(BOARD_DRAM_BASE)
cflags-y  += -DBOARD_RAM_END=$(BOARD_DRAM_END)

# The image stages (platform.md 6.2): image-y lists the stages `all` makes
# after pb.bin, each added through its Kconfig option, and FINAL_OUTPUT is the
# last file they make. Both are set while the makefiles are read, the board's
# before the platform's, so a board's own stage and FINAL_OUTPUT are kept.
# Before 3.0.0 the list was plat-y: a board that still fills it would build
# without its image, so it stops here.
ifneq ($(strip $(plat-y)),)
$(error plat-y ($(strip $(plat-y))) is image-y since 3.0.0: the platform's makefile.mk has the SoC's image stages, see platform.md 6.2 for what a board's makefile.mk still sets)
endif

FINAL_OUTPUT ?= $(BUILD_DIR)/$(TARGET).bin

ldflags-y += -Map=$(BUILD_DIR)/pb.map
ldflags-y += $(foreach v,$(BOARD_MEMORY),--defsym=$(v)=$($(v)))
ldflags-y += --defsym=PB_STACK_SIZE_KiB=$(CONFIG_STACK_SIZE_KiB)
ldflags-y += -T$(CONFIG_LINK_FILE) --build-id=none
ldflags-y += --gc-sections
# Every section is placed by a link script (platform.md 5.3): ld warns about
# one that isn't, which it would put where it chooses
ldflags-y += --orphan-handling=warn

OBJS =
OBJS += $(patsubst %.c, $(BUILD_DIR)/%.o, $(src-y))
OBJS += $(patsubst %.S, $(BUILD_DIR)/%.o, $(asm-y))

# A blob's object is $(BUILD_DIR)/<its name>.bino, wherever the blob is: the
# link reads it there (F-431)
BLOB_OBJS = $(patsubst %.bin, $(BUILD_DIR)/%.bino, $(notdir $(blobs-y)))

DEPS      = $(OBJS:.o=.d)

.PHONY: menuconfig olddefconfig

menuconfig olddefconfig:
	$(Q)BOARD=$(BOARD) $(PYTHON) scripts/$@.py src/Kconfig

all: $(BUILD_DIR)/$(TARGET).bin $(image-y)
	$(Q)$(SIZE) -x -t -B $(BUILD_DIR)/$(TARGET)
	$(if $(DEV_KEYS),@echo "Keys: the development keys (pki/dev-keys.mk) are never a product's")
	@echo "Success, final output: $(FINAL_OUTPUT)"

# Every object can use PB_VERSION
$(OBJS): $(call value_file,PB_VERSION)

# What the objects are compiled with and pb is linked with: a changed flag
# (CONFIG_OPTIMIZE on the command line, an edited makefile.mk), compiler or
# list of objects makes them again, and so does a changed link script (F-398)
compile_cmd = $(CC) $(cflags-y)
link_cmd = $(LD) $(ldflags-y) $(OBJS) $(BLOB_OBJS) $(LIBS)
LINK_SCRIPTS = $(call unquote,$(patsubst -T%,%,$(filter -T%,$(ldflags-y))))

$(OBJS): $(call value_file,compile_cmd)

$(BUILD_DIR)/keystore.o: $(KEY_PATHS) $(call value_file,KEYSTORE_ARGS) scripts/gen-keystore.py
	@echo "GEN $(BUILD_DIR)/keystore.c$(if $(DEV_KEYS), with the development keys (pki/dev-keys.mk))"
	$(Q)$(PYTHON) scripts/gen-keystore.py $(KEYSTORE_ARGS) > $(BUILD_DIR)/keystore.c.tmp
	$(Q)mv $(BUILD_DIR)/keystore.c.tmp $(BUILD_DIR)/keystore.c
	$(Q)$(CC) -c $(cflags-y) $(BUILD_DIR)/keystore.c -o $(BUILD_DIR)/keystore.o

SRK_FUSE_FILE := $(patsubst "%",%,$(CONFIG_SRK_FUSE))

$(BUILD_DIR)/srk_fuse.o: $(SRK_FUSE_FILE) $(call value_file,CONFIG_SRK_FUSE) scripts/gen-srk-fuse.py
	@echo GEN $(BUILD_DIR)/srk_fuse.c
	$(Q)$(PYTHON) scripts/gen-srk-fuse.py $(SRK_FUSE_FILE) > $(BUILD_DIR)/srk_fuse.c
	$(Q)$(CC) -c $(cflags-y) $(BUILD_DIR)/srk_fuse.c -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET)
	@echo OBJCOPY $< $@
	$(Q)cp $< $<_unstripped
	$(Q)$(STRIP) --strip-all $<
	$(Q)$(OBJCOPY) -O binary -R .comment $< $@

$(BUILD_DIR)/$(TARGET): $(BUILD_DIR)/keystore.o $(OBJS) $(BLOB_OBJS) $(LINK_SCRIPTS) $(call value_file,link_cmd)
	@echo LD $@
	$(Q)$(link_cmd) -o $@

# A rule per blob, for the object the link reads: the pattern rule's target
# was the blob's path under $(BUILD_DIR), a file nothing made, so pb was
# linked again on every make (F-431). The copy in blobs/ gives the symbols a
# name without the blob's directory.
define blob_rule
$(BUILD_DIR)/$(notdir $(1:.bin=.bino)): $(1)
	@mkdir -p $$(@D)
	@mkdir -p blobs/
	@mkdir -p $$(BUILD_DIR)/blobs
	@cp $$< blobs/
	@echo BLOB $$< $$(notdir $$<)
	$$(Q)$$(OBJCOPY) -I binary -O $$(ARCH_OUTPUT) -B $$(ARCH) blobs/$$(notdir $$<) $$@
endef
$(foreach blob,$(blobs-y),$(eval $(call blob_rule,$(blob))))

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
