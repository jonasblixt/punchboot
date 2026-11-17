#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMX8M_COMMON

# The key slots the SoC has a revoke bit for (platform.md 4.1): HABv4, SRK_REVOKE[2:0].
# plat_rot.revoke_slots, and the build warns for a board key above them
PLAT_REVOKE_SLOTS = 3
cflags-y += -DPLAT_REVOKE_SLOTS=$(PLAT_REVOKE_SLOTS)

# pb's buffers (platform.md 5.1), in the DRAM, after the DDR controller's state: the board's makefile.mk,
# read before this one, may put them elsewhere
PB_BUFFERS_BASE ?= 0x40060000
PB_BUFFERS_SIZE ?= 0x1000000

# Two stages (D-088, plat-imx.md 2): the SPL brings up DRAM, pb proper is
# linked at the board's PB_ENTRY in DRAM, PB_SIZE long
cflags-y += -DPB_HAS_SPL

src-y  += src/drivers/fuse/imx_hab_fuses.c
src-y  += src/plat/imx8m/plat.c
src-y  += src/plat/imx8m/tzasc.c
src-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/plat_imx8mq.c
src-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/plat_imx8mp.c

# The SPL: the DRAM init, the TZASC, the tick, the RAM log, the libc that
# LOG_* needs. The board adds its DRAM sources (board.md 7)
spl-src-y += src/plat/imx8m/spl/spl.c
spl-src-y += src/plat/imx8m/spl/ivt.c
spl-src-y += src/plat/imx8m/spl/hab.c
spl-src-y += src/plat/imx8m/ddr.c
spl-src-y += src/plat/imx8m/tzasc.c
spl-src-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/spl/spl_imx8mq.c
spl-src-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/plat_imx8mq.c
spl-src-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/ddr_imx8mq.c
spl-src-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/spl/spl_imx8mp.c
spl-src-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/plat_imx8mp.c
spl-src-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/ddr_imx8mp.c
spl-src-y += src/drivers/ddr/umctl2.c
spl-src-y += src/drivers/ddr/dwc_ddrphy.c
spl-src-y += src/drivers/tzc/tzc380.c
spl-src-y += src/drivers/timer/imx_gpt.c
spl-src-y += src/drivers/wdog/imx_wdog.c
spl-src-y += src/drivers/pinctrl/imx_iomuxc.c
spl-src-y += src/delay.c
spl-src-y += src/log.c
spl-src-y += src/console.c
spl-src-$(CONFIG_RAMLOG) += src/ramlog.c
spl-src-y += src/lib/libc/printf.c
spl-src-y += src/lib/libc/putchar.c
spl-src-y += src/lib/libc/string.c
spl-src-y += src/lib/libc/memset.c
spl-src-y += src/lib/libc/memmove.c
spl-src-y += src/lib/libc/strlen.c
spl-src-y += src/lib/libc/memcmp.c
spl-asm-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/spl/entry_imx8mq.S
spl-asm-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/spl/entry_imx8mp.S
spl-asm-y += src/arch/armv8a/misc_helpers.S
# USB recovery on the i.MX8MQ (D-089, plat-imx.md 2): its ROM's serial
# downloader can't load pb proper after the plugin, so the SPL is a fastboot
# device there, on the port and the stack command mode uses. A board without
# that port has no USB recovery
ifdef CONFIG_PLAT_IMX8MQ
ifdef CONFIG_IMX8M_CM_USB1
spl-cflags-y += -DIMX8M_SPL_FASTBOOT
# The fastboot device's product id, with pb's vendor id: the board's
# makefile.mk, read before this one, or the command line may give its own
ifdef PB_SPL_USB_PID
spl-cflags-y += -DPB_SPL_USB_PID=$(PB_SPL_USB_PID)
endif
spl-src-y += src/plat/imx8m/spl/fb_proto.c
spl-src-y += src/plat/imx8m/spl/fb_usb.c
spl-src-y += src/drivers/usb/usbd.c
spl-src-y += src/drivers/usb/dwc3_udc.c
spl-src-y += src/drivers/usb/imx8m_phy.c
spl-src-y += src/dma.c
endif
endif
# The boards' timing files as NXP's DDR tool writes them, with U-Boot's
# includes (imx8m-ddr.md)
spl-cflags-y += -I src/plat/imx8m/spl/compat
spl-asm-y += src/arch/armv8a/cache-ops.S

# NXP's LPDDR4 PHY training firmware (imx8m-ddr.md), linked in under these
# names: ddr.c uses the symbols they give. It isn't in the tree (NXP's EULA).
# The board's makefile.mk, read before this one, or the command line may name
# other files; scripts/ci/build-firmware.sh gives them from IMX8M_FW_DIR.
IMX8M_UMCTL_TRAIN_1D_D ?= lpddr4_pmu_train_1d_dmem.bin
IMX8M_UMCTL_TRAIN_1D_I ?= lpddr4_pmu_train_1d_imem.bin
IMX8M_UMCTL_TRAIN_2D_D ?= lpddr4_pmu_train_2d_dmem.bin
IMX8M_UMCTL_TRAIN_2D_I ?= lpddr4_pmu_train_2d_imem.bin

spl-blobs-y += $(IMX8M_UMCTL_TRAIN_1D_D) \
           $(IMX8M_UMCTL_TRAIN_1D_I) \
           $(IMX8M_UMCTL_TRAIN_2D_D) \
           $(IMX8M_UMCTL_TRAIN_2D_I)

# The ROM's load window, the on-chip RAM the SPL is loaded to and runs from
# (plat-imx.md 5, D-088): from IMX8M_SPL_ENTRY to IMX8M_LOAD_END. At its end
# the RAM log, which pb proper keeps, and TF-A's bl_params (platform.md 5.3),
# PLAT_BL_PARAMS_BASE, 4 KiB of which PLAT_BL_PARAMS_SIZE are used; the SPL
# ends where the RAM log starts. IMX8M_OCRAM_SIZE: the OCRAM, to map.
# IMX8M_ROM_SIZE: the boot ROM to map, for the ROM's APIs (HAB).
ifdef CONFIG_PLAT_IMX8MQ
# Unchecked (plat-imx.md 5): what the link script had for the board in the
# tree, 224 KiB of the TCM from its entry
IMX8M_SPL_ENTRY = 0x7e1000
IMX8M_LOAD_END = 0x819000
# Upstream TF-A's i.MX8MQ BL31 takes no bl_params (v2.12 to v2.15); the test
# bench's patch takes them from the TCM (0x7e0000, 256 KiB) or the OCRAM
# below BL31
PLAT_BL_PARAMS_BASE = 0x818000
PLAT_BL_PARAMS_LIMIT = 0x820000
IMX8M_OCRAM_SIZE = 0x20000
# The ROM's 124 KiB; the 4 KiB after them fault, a protected page (read on
# the pico8ml), as the 8M Plus's last 4 KiB
cflags-y += -DIMX8M_ROM_SIZE=0x1f000
endif
# i.MX8M Plus: the OCRAM free area after the ROM's part (RM, Figure 6-2), up
# to 0x97ffff; BL31 at 0x970000 takes bl_params from below 0x960000
ifdef CONFIG_PLAT_IMX8MP
# pb proper's part starts this far into pb.imx, after the SPL's (D-088)
IMX8MP_PB_OFFSET = 0x40000
spl-cflags-y += -DIMX8MP_PB_OFFSET=$(IMX8MP_PB_OFFSET)
IMX8M_SPL_ENTRY = 0x920000
IMX8M_LOAD_END = 0x960000
PLAT_BL_PARAMS_BASE = 0x95f000
PLAT_BL_PARAMS_LIMIT = 0x960000
IMX8M_OCRAM_SIZE = 0x90000
# The boot ROM is 252 KiB, then 4 KiB protected (RM, memory map)
cflags-y += -DIMX8M_ROM_SIZE=0x3f000
endif
IMX8M_RAMLOG_SIZE = $(if $(CONFIG_RAMLOG),$(shell printf '0x%x' $$(( $(CONFIG_RAMLOG_SIZE_KiB) * 1024 ))),0)
IMX8M_RAMLOG_BASE = $(shell printf '0x%x' $$(( $(PLAT_BL_PARAMS_BASE) - $(IMX8M_RAMLOG_SIZE) )))
IMX8M_SPL_STACK_SIZE = 0x2000
ldflags-y += --defsym=IMX8M_OCRAM_SIZE=$(IMX8M_OCRAM_SIZE)
ldflags-y += --defsym=IMX8M_RAMLOG_BASE=$(IMX8M_RAMLOG_BASE)
ldflags-y += --defsym=IMX8M_RAMLOG_SIZE=$(IMX8M_RAMLOG_SIZE)
cflags-y += -DIMX8M_OCRAM_SIZE=$(IMX8M_OCRAM_SIZE)
cflags-y += -DIMX8M_SPL_ENTRY=$(IMX8M_SPL_ENTRY)

# TF-A's bl_params (platform.md 5.3): plat.c has the memory, plat_bl_params,
# of PLAT_BL_PARAMS_SIZE bytes at PLAT_BL_PARAMS_BASE, and link.lds checks
# that it ends below the limit.
PLAT_BL_PARAMS_SIZE = 512
ifdef CONFIG_BOOT_ATF
cflags-y += -DPLAT_BL_PARAMS_SIZE=$(PLAT_BL_PARAMS_SIZE)
endif
ldflags-y += --defsym=PLAT_BL_PARAMS_BASE=$(PLAT_BL_PARAMS_BASE)
ldflags-y += --defsym=PLAT_BL_PARAMS_SIZE=$(PLAT_BL_PARAMS_SIZE)
ldflags-y += --defsym=PLAT_BL_PARAMS_LIMIT=$(PLAT_BL_PARAMS_LIMIT)

# The SPL's link (spl/link.lds)
spl-ldflags-y += -Map=$(BUILD_DIR)/spl.map --build-id=none --gc-sections --orphan-handling=warn -z noexecstack
spl-ldflags-y += -z max-page-size=4096 --no-warn-rwx-segments
spl-ldflags-y += --defsym=IMX8M_SPL_ENTRY=$(IMX8M_SPL_ENTRY)
spl-ldflags-y += --defsym=IMX8M_SPL_END=$(IMX8M_RAMLOG_BASE)
spl-ldflags-y += --defsym=IMX8M_RAMLOG_BASE=$(IMX8M_RAMLOG_BASE)
spl-ldflags-y += --defsym=IMX8M_RAMLOG_SIZE=$(IMX8M_RAMLOG_SIZE)
spl-ldflags-y += --defsym=IMX8M_SPL_STACK_SIZE=$(IMX8M_SPL_STACK_SIZE)
spl-ldflags-y += --defsym=PB_BUFFERS_BASE=$(PB_BUFFERS_BASE)
ifdef CONFIG_PLAT_IMX8MQ
spl-ldflags-y += --defsym=SPL_ENTRY_SYMBOL=spl_plugin_entry
else
spl-ldflags-y += --defsym=SPL_ENTRY_SYMBOL=spl_entry
endif
spl-ldflags-y += -Tsrc/plat/imx8m/spl/link.lds

cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x100000000
ldflags-y += -Tsrc/plat/imx8m/link.lds

src-y  += src/drivers/pinctrl/imx_iomuxc.c

# The image stages (platform.md 6.2, plat-imx.md 6). The inputs are set with
# ?=: the board's makefile.mk, read before this one, or the command line may
# give its own.
ifdef CONFIG_PLAT_IMX8MQ
# NXP's mkimage_imx8m (imx-mkimage)
MKIMAGE ?= mkimage_imx8m
else
# U-Boot's mkimage
MKIMAGE ?= mkimage
endif
CST_TOOL ?= cst
# The i.MX8M Plus: the boot device in the image's header
IMX8M_BOOT_FROM ?= sd
# The keys, from the defconfig
IMX8M_SRK_TABLE ?= $(call unquote,$(CONFIG_IMX8M_SRK_TABLE))
IMX8M_KEY_INDEX ?= $(CONFIG_IMX8M_KEY_INDEX)
IMX8M_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX8M_SIGN_CERT))
IMX8M_IMAGE_SIGN_CERT ?= $(call unquote,$(CONFIG_IMX8M_IMAGE_SIGN_CERT))

IMX8M_CSF_TEMPLATE = src/plat/imx8m/pb.csf.template

.PHONY: imx8m_image imx8m_sign_desc imx8m_sign_image

ifdef CONFIG_PLAT_IMX8MQ
# pb.imx (D-088, plat-imx.md 6): the SPL as the HABv4 plugin, pb proper
# (pb.bin padded to 512 bytes, pb_pad.bin) as the loader, made with
# mkimage_imx8m. The plugin's CSF has 8 KiB (spl_csf_room.bin), pb proper's
# the 8 KiB mkimage_imx8m leaves at the end. mkimage_imx8m's offsets, from
# its output (pb.imx.log), are in pb.imx.offsets for the signing: the
# plugin's CSF, pb proper's IVT and its CSF. pb_pad.imx is the image that is
# signed. The ROM reads the plugin's IVT at the SPL's entry - 0x40, pb
# proper's at PB_ENTRY - 0x40, and loads pb proper's from the image's start
# (0x400 before the IVT): that is checked to be after pb's buffers. The
# SPL's fastboot (D-089) downloads the image to where pb proper's IVT is at
# its address, memory the USB controller writes: checked to be on a cache
# line.
IMX8M_CSF_ROOM = 0x2000

imx8m_image: $(BUILD_DIR)/pb.bin $(BUILD_DIR)/spl.bin
	@echo Generating $(BUILD_DIR)/pb.imx
	@dd if=$(BUILD_DIR)/pb.bin of=$(BUILD_DIR)/pb_pad.bin bs=512 conv=sync,noerror 2> /dev/null
	@dd if=/dev/zero of=$(BUILD_DIR)/spl_csf_room.bin bs=$$(( $(IMX8M_CSF_ROOM) )) count=1 2> /dev/null
	$(Q)$(MKIMAGE) -plugin $(BUILD_DIR)/spl.bin $(IMX8M_SPL_ENTRY) \
	    -csf_plugin $(BUILD_DIR)/spl_csf_room.bin \
	    -loader $(BUILD_DIR)/pb_pad.bin $(PB_ENTRY) -out $(BUILD_DIR)/pb.imx > $(BUILD_DIR)/pb.imx.log 2>&1 || \
	    { cat $(BUILD_DIR)/pb.imx.log; exit 1; }
	$(Q)off() { sed -n "s/^ *$$1[ \t]*\(0x[0-9a-fA-F]*\).*/\1/p" $(BUILD_DIR)/pb.imx.log | head -1; }; \
	 spl_csf=$$(off csf_plugin_off); image=$$(off header_image_off); csf=$$(off csf_off); \
	 [ -n "$$spl_csf" ] && [ -n "$$image" ] && [ -n "$$csf" ] || \
	     { echo "pb.imx: no offsets in $(BUILD_DIR)/pb.imx.log"; exit 1; }; \
	 start=$$(( $(PB_ENTRY) - 0x40 - $$image - 0x400 )); \
	 [ $$start -ge $$(( $(PB_BUFFERS_BASE) + $(PB_BUFFERS_SIZE) )) ] || \
	     { printf 'pb.imx: pb proper is loaded from 0x%x, in pb'"'"'s buffers: PB_ENTRY is too low\n' $$start; exit 1; }; \
	 [ $$(( ($(PB_ENTRY) - 0x40 - $$image) % 64 )) -eq 0 ] || \
	     { echo "pb.imx: the SPL's fastboot (D-089) downloads the image to PB_ENTRY - 0x40 - $$image, which isn't on a cache line"; exit 1; }; \
	 printf 'SPL_CSF=%s\nIMAGE=%s\nCSF=%s\n' $$spl_csf $$image $$csf > $(BUILD_DIR)/pb.imx.offsets
	@cp $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_pad.imx
	@echo "$(BUILD_DIR)/pb_pad.imx ready"

# spl.csf, the plugin's HABv4 description: the plugin from its IVT to its
# CSF, and pb proper's IVT, which the ROM loads with it into the TCM, at the
# SPL's entry - 0x40 + its offset (D-088). Without the [Unlock]s: they are
# pb proper's, the image HAB exits into. pb.csf, pb proper's: from its IVT to
# its CSF.
imx8m_sign_desc: imx8m_image
	$(Q). $(BUILD_DIR)/pb.imx.offsets; \
	 printf '    Blocks = 0x%X 0x0 0x%X "%s", \\\n             0x%X 0x%X 0x40 "%s"\n' \
	        $$(( $(IMX8M_SPL_ENTRY) - 0x40 )) $$(( SPL_CSF )) $(BUILD_DIR)/pb_pad.imx \
	        $$(( $(IMX8M_SPL_ENTRY) - 0x40 + IMAGE )) $$(( IMAGE )) $(BUILD_DIR)/pb_pad.imx \
	        > $(BUILD_DIR)/spl.csf.blocks; \
	 printf '    Blocks = 0x%X 0x%X 0x%X "%s"\n' \
	        $$(( $(PB_ENTRY) - 0x40 )) $$(( IMAGE )) $$(( CSF - IMAGE )) $(BUILD_DIR)/pb_pad.imx \
	        > $(BUILD_DIR)/pb.csf.blocks; \
	 subst() { sed -e "/__BLOCKS__/{r $$1" -e 'd;}' \
	     -e 's#__KEY_INDEX__#$(IMX8M_KEY_INDEX)#g' \
	     -e 's#__SRK_TBL__#$(IMX8M_SRK_TABLE)#g' \
	     -e 's#__CSFK_PEM__#$(IMX8M_SIGN_CERT)#g' \
	     -e 's#__IMG_PEM__#$(IMX8M_IMAGE_SIGN_CERT)#g' $(IMX8M_CSF_TEMPLATE); }; \
	 subst $(BUILD_DIR)/spl.csf.blocks | sed -e '/^\[Unlock\]/,/^$$/d' > $(BUILD_DIR)/spl.csf; \
	 subst $(BUILD_DIR)/pb.csf.blocks > $(BUILD_DIR)/pb.csf

# The ROM reads the IVT 33 KiB into an SD/MMC/eMMC device, a boot partition
# too (8MQ RM, Table "IVT offset"; the 8M Plus reads a boot partition at 0)
IMX8MQ_IMAGE_OFFSET_KiB = 33

# pb_signed_uuu.imx: pb_pad.imx with both signatures where mkimage_imx8m left
# room for them; for the serial downloader (uuu), which sends the plugin,
# then pb proper. pb_signed.imx: the same after IMX8MQ_IMAGE_OFFSET_KiB of
# zeros, for the boot partition or the card (part write, F-448). The zeros
# aren't loaded: the signatures are both's
imx8m_sign_image: imx8m_sign_desc
	@$(CST_TOOL) -i $(BUILD_DIR)/spl.csf -o $(BUILD_DIR)/spl_csf.bin
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/pb_csf.bin
	$(Q)for f in spl_csf.bin pb_csf.bin; do \
	     [ $$(stat -c%s $(BUILD_DIR)/$$f) -le $$(( $(IMX8M_CSF_ROOM) )) ] || \
	         { echo "$(BUILD_DIR)/$$f is larger than its room in pb.imx"; exit 1; }; done
	@cp $(BUILD_DIR)/pb_pad.imx $(BUILD_DIR)/pb_signed_uuu.imx
	$(Q). $(BUILD_DIR)/pb.imx.offsets; \
	 dd if=$(BUILD_DIR)/spl_csf.bin of=$(BUILD_DIR)/pb_signed_uuu.imx bs=1 conv=notrunc \
	    seek=$$(( SPL_CSF )) 2> /dev/null && \
	 dd if=$(BUILD_DIR)/pb_csf.bin of=$(BUILD_DIR)/pb_signed_uuu.imx bs=1 conv=notrunc \
	    seek=$$(( CSF )) 2> /dev/null
	$(Q)dd if=/dev/zero of=$(BUILD_DIR)/pb_signed.imx bs=1024 count=$(IMX8MQ_IMAGE_OFFSET_KiB) 2> /dev/null
	$(Q)cat $(BUILD_DIR)/pb_signed_uuu.imx >> $(BUILD_DIR)/pb_signed.imx
else
# pb.imx (D-088, plat-imx.md 6), for the boot media or the serial downloader
# (uuu): spl.imx, the SPL with the header of the i.MX8M Plus ROM
# (ROM_VERSION v2), padded to IMX8MP_PB_OFFSET, then pb proper's part
# (pb_part.bin, scripts/gen-imx8m-ivt.py): its IVT, pb.bin and room for a
# CSF. The SPL reads pb proper there with the ROM's API.
# mkimage's offsets, from its output (spl.imx.log), and pb proper's are in
# pb.imx.offsets for the signing: the SPL's address, its length up to its
# CSF, which mkimage leaves 8 KiB for at the end of spl.imx, and pb proper's
# part and its CSF in pb.imx.
IMX8M_CSF_ROOM = 0x2000

imx8m_image: $(BUILD_DIR)/pb.bin $(BUILD_DIR)/spl.bin scripts/gen-imx8m-ivt.py
	@echo Generating $(BUILD_DIR)/pb.imx
	@printf 'ROM_VERSION v2\nBOOT_FROM $(IMX8M_BOOT_FROM)\nLOADER $(BUILD_DIR)/spl.bin $(IMX8M_SPL_ENTRY)\n' > $(BUILD_DIR)/spl.cfg
	$(Q)$(MKIMAGE) -n $(BUILD_DIR)/spl.cfg -T imx8mimage -e $(IMX8M_SPL_ENTRY) -d $(BUILD_DIR)/spl.bin $(BUILD_DIR)/spl.imx > $(BUILD_DIR)/spl.imx.log 2>&1 || \
	    { cat $(BUILD_DIR)/spl.imx.log; exit 1; }
	$(Q)[ $$(stat -c%s $(BUILD_DIR)/spl.imx) -le $$(( $(IMX8MP_PB_OFFSET) )) ] || \
	    { echo "$(BUILD_DIR)/spl.imx is larger than IMX8MP_PB_OFFSET"; exit 1; }
	$(Q)$(PYTHON) scripts/gen-imx8m-ivt.py $(PB_ENTRY) $(IMX8M_CSF_ROOM) $(BUILD_DIR)/pb.bin $(BUILD_DIR)/pb_part.bin
	$(Q)cp $(BUILD_DIR)/spl.imx $(BUILD_DIR)/pb.imx
	$(Q)truncate -s $$(( $(IMX8MP_PB_OFFSET) )) $(BUILD_DIR)/pb.imx
	$(Q)cat $(BUILD_DIR)/pb_part.bin >> $(BUILD_DIR)/pb.imx
	$(Q)set -- $$(sed -n 's/^ *spl hab block:[ \t]*//p' $(BUILD_DIR)/spl.imx.log | head -1); \
	 spl_self=$$1; spl_off=$$2; spl_csf=$$3; \
	 [ -n "$$spl_self" ] && [ -n "$$spl_csf" ] && [ $$(( spl_off )) -eq 0 ] || \
	     { echo "pb.imx: no SPL block in $(BUILD_DIR)/spl.imx.log"; exit 1; }; \
	 [ $$(( spl_csf + $(IMX8M_CSF_ROOM) )) -le $$(( $(IMX8MP_PB_OFFSET) )) ] || \
	     { echo "pb.imx: the SPL's CSF doesn't fit below IMX8MP_PB_OFFSET"; exit 1; }; \
	 csf=$$(( $(IMX8MP_PB_OFFSET) + $$(stat -c%s $(BUILD_DIR)/pb_part.bin) - $(IMX8M_CSF_ROOM) )); \
	 printf 'SPL_SELF=%s\nSPL_CSF=%s\nIMAGE=%s\nCSF=0x%x\n' $$spl_self $$spl_csf $(IMX8MP_PB_OFFSET) $$csf \
	     > $(BUILD_DIR)/pb.imx.offsets

# spl.csf, the SPL's HABv4 description: from its IVT to its CSF, which the
# ROM authenticates. pb.csf, pb proper's: from its IVT to its CSF, which the
# SPL has HAB authenticate (D-088). Both with the [Unlock]s: the ROM leaves
# HAB after the SPL, and what it locks then stays locked (NXP's csf_spl.txt
# has them in the SPL's too).
imx8m_sign_desc: imx8m_image
	$(Q). $(BUILD_DIR)/pb.imx.offsets; \
	 printf '    Blocks = 0x%X 0x0 0x%X "%s"\n' \
	        $$(( SPL_SELF )) $$(( SPL_CSF )) $(BUILD_DIR)/pb.imx > $(BUILD_DIR)/spl.csf.blocks; \
	 printf '    Blocks = 0x%X 0x%X 0x%X "%s"\n' \
	        $$(( $(PB_ENTRY) - 0x40 )) $$(( IMAGE )) $$(( CSF - IMAGE )) $(BUILD_DIR)/pb.imx \
	        > $(BUILD_DIR)/pb.csf.blocks; \
	 subst() { sed -e "/__BLOCKS__/{r $$1" -e 'd;}' \
	     -e 's#__KEY_INDEX__#$(IMX8M_KEY_INDEX)#g' \
	     -e 's#__SRK_TBL__#$(IMX8M_SRK_TABLE)#g' \
	     -e 's#__CSFK_PEM__#$(IMX8M_SIGN_CERT)#g' \
	     -e 's#__IMG_PEM__#$(IMX8M_IMAGE_SIGN_CERT)#g' $(IMX8M_CSF_TEMPLATE); }; \
	 subst $(BUILD_DIR)/spl.csf.blocks > $(BUILD_DIR)/spl.csf; \
	 subst $(BUILD_DIR)/pb.csf.blocks > $(BUILD_DIR)/pb.csf

# pb_signed.imx: pb.imx with both signatures where the image has room for
# them; for the boot media and the serial downloader (uuu) alike
imx8m_sign_image: imx8m_sign_desc
	@$(CST_TOOL) -i $(BUILD_DIR)/spl.csf -o $(BUILD_DIR)/spl_csf.bin
	@$(CST_TOOL) -i $(BUILD_DIR)/pb.csf -o $(BUILD_DIR)/pb_csf.bin
	$(Q)for f in spl_csf.bin pb_csf.bin; do \
	     [ $$(stat -c%s $(BUILD_DIR)/$$f) -le $$(( $(IMX8M_CSF_ROOM) )) ] || \
	         { echo "$(BUILD_DIR)/$$f is larger than its room in pb.imx"; exit 1; }; done
	@cp $(BUILD_DIR)/pb.imx $(BUILD_DIR)/pb_signed.imx
	$(Q). $(BUILD_DIR)/pb.imx.offsets; \
	 dd if=$(BUILD_DIR)/spl_csf.bin of=$(BUILD_DIR)/pb_signed.imx bs=1 conv=notrunc \
	    seek=$$(( SPL_CSF )) 2> /dev/null && \
	 dd if=$(BUILD_DIR)/pb_csf.bin of=$(BUILD_DIR)/pb_signed.imx bs=1 conv=notrunc \
	    seek=$$(( CSF )) 2> /dev/null
endif

image-$(CONFIG_IMX8M_CREATE_IMX_IMAGE) += imx8m_image
image-$(CONFIG_IMX8M_SIGN_IMAGE) += imx8m_sign_desc imx8m_sign_image

# What `all` names as the build's output, unless the board names its own
ifdef CONFIG_IMX8M_SIGN_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb_signed.imx
else ifdef CONFIG_IMX8M_CREATE_IMX_IMAGE
FINAL_OUTPUT ?= $(BUILD_DIR)/pb.imx
endif
endif
