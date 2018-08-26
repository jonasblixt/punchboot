PB_BOARD_NAME = Bebop
PB_PLAT_NAME   = imx6ul
PB_ENTRY     = 0x80001000

CFLAGS += -I board/bebop

BOARD_C_SRCS += board/bebop/bebop.c



MKIMAGE         = mkimage
IMX_USB         = imx_usb
BEBOP_IMAGE_CFG = board/bebop/imximage.cfg
FINAL_IMAGE     = $(TARGET).imx

board_final: $(TARGET).bin
	@echo FINAL $(TARGET).imx
	@$(MKIMAGE) -n $(BEBOP_IMAGE_CFG) -T imximage -e $(PB_ENTRY) \
			-d $(TARGET).bin $(TARGET).imx > /dev/null

board_clean:
	@-rm -rf board/bebop/*.o 
	@-rm -rf *.imx

prog:
	@$(IMX_USB) $(TARGET).imx
