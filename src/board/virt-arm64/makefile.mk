PB_ENTRY     = 0x60000000

cflags-y += -DBOARD_RAM_BASE=0x60000000
cflags-y += -DBOARD_RAM_END=0x80000000

src-y += $(BOARD)/board.c
