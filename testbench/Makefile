# Linux test bench for the boards that run Linux, a Buildroot BR2_EXTERNAL
# tree built out of tree.
#
#   make                      imx8mpevk, in output/imx8mpevk
#   make BOARD=jiffy          configs/jiffy_defconfig, in output/jiffy
#   make BOARD=jiffy ADDON=optee
#                             an add-on: configs/jiffy_defconfig and
#                             configs/jiffy-optee.fragment, in
#                             output/jiffy-optee
#
# Other targets are passed to Buildroot, e.g.
# `make BOARD=jiffy menuconfig`, `make BOARD=jiffy savedefconfig`.

# Latest Buildroot LTS
BR_VERSION = 2026.02.3
BR_SHA256 = 5a59e7501b0b4ec52c41f4bfa79412320e0b37eae5f719605a258e8d0c6fc7fb
BR_TARBALL = buildroot-$(BR_VERSION).tar.xz
BR_URL = https://buildroot.org/downloads/$(BR_TARBALL)

TOP := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BR_DIR = $(TOP)/buildroot-$(BR_VERSION)
BOARD ?= imx8mpevk
ADDON ?=
O ?= $(TOP)/output/$(BOARD)$(if $(ADDON),-$(ADDON))
DEFCONFIG ?= $(BOARD)_defconfig

# Shared between Buildroot versions and boards
export BR2_DL_DIR ?= $(TOP)/dl

BR_MAKE = $(MAKE) -C $(BR_DIR) O=$(O) BR2_EXTERNAL=$(TOP)

all: $(O)/.config
	$(BR_MAKE)

ifeq ($(ADDON),)
$(O)/.config: | $(BR_DIR)/Makefile
	$(BR_MAKE) $(DEFCONFIG)
else
# The board's defconfig with the add-on's fragment after it: a later value
# wins
ADDON_DEFCONFIG = $(O)/addon_defconfig
$(O)/.config: $(TOP)/configs/$(DEFCONFIG) $(TOP)/configs/$(BOARD)-$(ADDON).fragment | $(BR_DIR)/Makefile
	mkdir -p $(O)
	cat $^ > $(ADDON_DEFCONFIG)
	$(BR_MAKE) BR2_DEFCONFIG=$(ADDON_DEFCONFIG) defconfig
endif

$(DEFCONFIG): | $(BR_DIR)/Makefile
	$(BR_MAKE) $(DEFCONFIG)

$(BR_DIR)/Makefile:
	mkdir -p $(BR2_DL_DIR)
	cd $(BR2_DL_DIR) && \
		if [ ! -f $(BR_TARBALL) ]; then \
			wget -O $(BR_TARBALL).tmp $(BR_URL) && mv $(BR_TARBALL).tmp $(BR_TARBALL); \
		fi && \
		echo "$(BR_SHA256)  $(BR_TARBALL)" | sha256sum -c -
	tar -xf $(BR2_DL_DIR)/$(BR_TARBALL) -C $(TOP)

buildroot: $(BR_DIR)/Makefile

distclean:
	rm -rf $(O)

# Keep the catch-all rule below from trying to remake this Makefile
Makefile: ;

%: | $(BR_DIR)/Makefile
	$(BR_MAKE) $@

.PHONY: all buildroot distclean $(DEFCONFIG)
