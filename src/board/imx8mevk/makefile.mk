src-y += $(BOARD)/board.c
src-y += $(BOARD)/lpddr4_timing.c

IMX8M_UMCTL_TRAIN_1D_D ?= lpddr4_pmu_train_1d_dmem.bin
IMX8M_UMCTL_TRAIN_1D_I ?= lpddr4_pmu_train_1d_imem.bin
IMX8M_UMCTL_TRAIN_2D_D ?= lpddr4_pmu_train_2d_dmem.bin
IMX8M_UMCTL_TRAIN_2D_I ?= lpddr4_pmu_train_2d_imem.bin

blobs-y  = $(IMX8M_UMCTL_TRAIN_1D_D) \
		   $(IMX8M_UMCTL_TRAIN_1D_I)\
		   $(IMX8M_UMCTL_TRAIN_2D_D) \
		   $(IMX8M_UMCTL_TRAIN_2D_I)

# The image stages are the i.MX8M's (src/plat/imx8m/makefile.mk), turned on
# and given the keys in the defconfig
