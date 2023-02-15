PB_ENTRY     = 0x40000000
cflags-y += -I $(BOARD)/include
src-y += $(BOARD)/board.c

include tests/makefile.mk
