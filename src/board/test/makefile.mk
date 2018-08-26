PB_BOARD_NAME = Test
PB_PLAT_NAME   = test
PB_ENTRY     = 0x40000000

CFLAGS += -I board/test

BOARD_C_SRCS += board/test/test.c

board_clean:
	@-rm -rf board/test/*.o 

board_final:
