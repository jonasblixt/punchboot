PB_BOARD_NAME = imx8qxmek
PB_PLAT_NAME   = imx8x
PB_ENTRY     = 0x80200000

CFLAGS += -I board/imx8qxmek/include

BOARD_C_SRCS += board/imx8qxmek/imx8qxmek.c
BOARD_C_SRCS += boot/atf_linux.c

MKIMAGE         = mkimage_imx8
FINAL_IMAGE     = $(TARGET).imx

board_final: $(TARGET).bin
	@echo fixme board_final
board_clean:
	@-rm -rf board/imx8qxmek/*.o 

