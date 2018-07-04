PB_BOARD_NAME = Test
PB_PLAT_NAME   = test
PB_ENTRY     = 0x80000000

CFLAGS += -I board/test

C_SRCS += board/test/test.c

board_clean:
	@-rm -rf board/test/*.o 

board_final:
