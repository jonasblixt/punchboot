PB_BOARD_NAME = pico8ml
PB_PLAT_NAME   = imx8m
PB_ENTRY     = 0x7E1000

CFLAGS += -I board/pico8ml/include

BOARD_C_SRCS += board/pico8ml/pico8ml.c
BOARD_C_SRCS += board/pico8ml/ddr_init.c
BOARD_C_SRCS += board/pico8ml/ddrphy_train.c
BOARD_C_SRCS += board/pico8ml/helper.c


MKIMAGE         = mkimage_imx8
IMX_USB         = imx_usb
FINAL_IMAGE     = $(TARGET).imx

board_final: $(TARGET).bin
	@objcopy -I binary -O binary --pad-to 0x8000 --gap-fill=0x0 board/pico8ml/lpddr4_pmu_train_1d_imem.bin lpddr4_pmu_train_1d_imem_pad.bin
	@objcopy -I binary -O binary --pad-to 0x4000 --gap-fill=0x0 board/pico8ml/lpddr4_pmu_train_1d_dmem.bin lpddr4_pmu_train_1d_dmem_pad.bin
	@objcopy -I binary -O binary --pad-to 0x8000 --gap-fill=0x0 board/pico8ml/lpddr4_pmu_train_2d_imem.bin lpddr4_pmu_train_2d_imem_pad.bin
	@cat lpddr4_pmu_train_1d_imem_pad.bin lpddr4_pmu_train_1d_dmem_pad.bin > lpddr4_pmu_train_1d_fw.bin
	@cat lpddr4_pmu_train_2d_imem_pad.bin board/pico8ml/lpddr4_pmu_train_2d_dmem.bin > lpddr4_pmu_train_2d_fw.bin
	@cat $(TARGET).bin lpddr4_pmu_train_1d_fw.bin lpddr4_pmu_train_2d_fw.bin > $(TARGET)-lpddr4.bin
	@$(MKIMAGE) -csf pb.csf -loader $(TARGET)-lpddr4.bin $(PB_ENTRY) -out $(TARGET).imx 

board_clean:
	@-rm -rf board/pico8ml/*.o 
	@-rm -rf *.imx

prog:
	@$(IMX_USB) $(TARGET).imx
