PB_ENTRY     = 0x80000000
CFLAGS += -I $(BOARD)/include
BOARD_C_SRCS += $(BOARD)/board.c
FINAL_IMAGE     = $(TARGET).imx
