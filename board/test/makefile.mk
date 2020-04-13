ifdef CONFIG_BOARD_TEST

PB_BOARD_NAME = qemutest
PB_ENTRY     = 0x40000000

CFLAGS += -I board/test/include
CFLAGS += -g  -fprofile-arcs -ftest-coverage

BOARD_C_SRCS += board/test/board.c

endif
