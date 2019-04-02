PB_BOARD_NAME = Bebop
PB_PLAT_NAME   = imx6ul
PB_ENTRY     = 0x80001000

CFLAGS += -I board/bebop/include

BOARD_C_SRCS += board/bebop/bebop.c
BOARD_C_SRCS += boot/atf_linux.c


MKIMAGE         = mkimage
IMX_USB         = imx_usb
JIFFY_IMAGE_CFG = board/bebop/imximage.cfg
FINAL_IMAGE     = $(TARGET).imx

KEYS  = ../pki/secp521r1-pub-key.der
#KEYS = ../pki/dev_rsa_public.der
#KEYS += ../pki/field1_rsa_public.der
#KEYS += ../pki/field2_rsa_public.der

KEY_TYPE = EC
board_final: $(TARGET).bin
	@$(MKIMAGE) -n $(JIFFY_IMAGE_CFG) -T imximage -e $(PB_ENTRY) \
			-d $(TARGET).bin $(TARGET).imx 

board_clean:
	@-rm -rf board/bebop/*.o 
	@-rm -rf *.imx

prog:
	@$(IMX_USB) $(TARGET).imx
