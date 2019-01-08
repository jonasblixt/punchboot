PB_BOARD_NAME = pico8ml
PB_PLAT_NAME   = imx8m
PB_ENTRY     = 0x7E1000

CFLAGS += -I board/pico8ml/include

BOARD_C_SRCS += board/pico8ml/pico8ml.c



MKIMAGE         = mkimage_imx8
IMX_USB         = imx_usb
FINAL_IMAGE     = $(TARGET).imx

board_final: $(TARGET).bin
	@$(MKIMAGE) -fit -loader $(TARGET).bin $(PB_ENTRY) -out $(TARGET).imx 

board_clean:
	@-rm -rf board/pico8ml/*.o 
	@-rm -rf *.imx

prog:
	@$(IMX_USB) $(TARGET).imx
