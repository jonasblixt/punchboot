src-y += $(BOARD)/board.c

# The board's memory (platform.md 5.1): where pb is linked, pb's region,
# the DRAM and pb's buffers. The link checks them against the SoC.
# No DRAM: pb and its buffers are in the OCRAM, the buffers the platform's
PB_ENTRY        = 0x20249000
BOARD_DRAM_BASE = 0x0
BOARD_DRAM_SIZE = 0x0

# The image stages are the i.MX RT's (src/plat/imxrt/makefile.mk), turned on
# in the defconfig. The board's inputs are its imximage.cfg, which they find
# here, and the FlexSPI configuration block for the revision's QSPI NOR
# (Kconfig).
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_A) = $(BOARD)/fcb_is25wp064.c
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_B) = $(BOARD)/fcb_is25wp064.c
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_C) = $(BOARD)/fcb_w25q128jw.c
IMXRT_FCB_SRC ?= $(fcb-y)
