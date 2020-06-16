PB_ENTRY     = 0x40000000
CFLAGS += -I board/qemuarmv7/include
CFLAGS += -DPB_BOOT_TEST=1
BOARD_C_SRCS += board/qemuarmv7/board.c
