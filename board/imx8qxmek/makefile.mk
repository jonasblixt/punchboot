ifdef CONFIG_BOARD_IMX8QXMEK

PB_BOARD_NAME = imx8qxmek
PB_PLAT_NAME   = imx8x
PB_ENTRY     = 0x80000000

CFLAGS += -I board/imx8qxmek/include

BOARD_C_SRCS += board/imx8qxmek/board.c

MKIMAGE         = mkimage_imx8
FINAL_IMAGE     = $(TARGET).imx

endif
