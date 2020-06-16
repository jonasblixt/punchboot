PB_ENTRY     = 0x40000000
CFLAGS += -I board/test/include
BOARD_C_SRCS += board/test/board.c

include tests/makefile.mk
