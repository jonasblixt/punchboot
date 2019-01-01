PB_BOARD_NAME = Jiffy
PB_PLAT_NAME   = imx6ul
PB_ENTRY     = 0x80001000

CFLAGS += -I board/jiffy

BOARD_C_SRCS += board/jiffy/jiffy.c



MKIMAGE         = mkimage
IMX_USB         = imx_usb
JIFFY_IMAGE_CFG = board/jiffy/imximage.cfg
FINAL_IMAGE     = $(TARGET).imx

board_final: $(TARGET).bin
	@$(MKIMAGE) -n $(JIFFY_IMAGE_CFG) -T imximage -e $(PB_ENTRY) \
			-d $(TARGET).bin $(TARGET).imx 

board_clean:
	@-rm -rf board/jiffy/*.o 
	@-rm -rf *.imx

prog:
	@$(IMX_USB) $(TARGET).imx
