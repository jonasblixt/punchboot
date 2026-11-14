src-y += $(BOARD)/board.c

# The image stages are the i.MX RT's (src/plat/imxrt/makefile.mk), turned on
# in the defconfig. The board's inputs are its imximage.cfg, which they find
# here, and the FlexSPI configuration block for the revision's QSPI NOR
# (Kconfig).
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_A) = $(BOARD)/fcb_is25wp064.c
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_B) = $(BOARD)/fcb_is25wp064.c
fcb-$(CONFIG_BOARD_IMXRT1060EVK_REV_C) = $(BOARD)/fcb_w25q128jw.c
IMXRT_FCB_SRC ?= $(fcb-y)
