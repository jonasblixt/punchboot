src-y += $(BOARD)/board.c

# The image stages are the i.MX6UL's (src/plat/imx6ul/makefile.mk), turned on
# and given the keys in the defconfig. Their input from the board is its DCD,
# imximage.cfg, which they find here.
