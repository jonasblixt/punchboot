PB_BOARD_NAME = pico8ml
PB_PLAT_NAME   = imx8m
PB_ENTRY     = 0x7E1000

CFLAGS += -I board/pico8ml/include

BOARD_C_SRCS += board/pico8ml/pico8ml.c
BOARD_C_SRCS += board/pico8ml/ddr_init.c
BOARD_C_SRCS += board/pico8ml/ddrphy_train.c
BOARD_C_SRCS += board/pico8ml/helper.c


MKIMAGE         = mkimage_imx8_imx8m
IMX_USB         = imx_usb
FINAL_IMAGE     = $(TARGET).imx

board_final: $(TARGET).bin
	$(MKIMAGE) -loader $(TARGET).bin $(PB_ENTRY) -out $(TARGET).imx 
	$(eval PB_FILESIZE=$(shell stat -c%s "pb.imx"))
	$(eval PB_FILESIZE2=$(shell echo " $$(( $(PB_FILESIZE) - 0x2000 ))" | bc	))
	@dd if=pb_csf.bin of=pb.imx seek=$(PB_FILESIZE2) bs=1 conv=notrunc
board_clean:
	@-rm -rf board/pico8ml/*.o 
	@-rm -rf *.imx

prog:
	@$(IMX_USB) $(TARGET).imx
