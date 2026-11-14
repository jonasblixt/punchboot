src-y += $(BOARD)/board.c
src-y += $(BOARD)/lpddr4_timing.c

# The LPDDR4 training firmware is the platform's (src/plat/imx8m/makefile.mk)

# The image stages are the i.MX8M's (src/plat/imx8m/makefile.mk), turned on
# and given the keys in the defconfig
