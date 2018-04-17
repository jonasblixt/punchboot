PB_ARCH_NAME = armv7a-ve

C_SRCS  += soc/imx6ul/uart.c
C_SRCS  += soc/imx6ul/usb.c
C_SRCS  += soc/imx6ul/emmc.c
C_SRCS  += soc/imx6ul/reset.c


soc_clean:
	@-rm -rf soc/imx6ul/*.o
