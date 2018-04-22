PB_ARCH_NAME = armv7a-ve

C_SRCS  += plat/imx6ul/imx_uart.c
C_SRCS  += plat/imx6ul/ehci.c
C_SRCS  += plat/imx6ul/usdhc.c
C_SRCS  += plat/imx6ul/reset.c
C_SRCS  += plat/imx6ul/gpt.c
C_SRCS  += plat/imx6ul/caam.c

plat_clean:
	@-rm -rf plat/imx6ul/*.o
