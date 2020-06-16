PB_ENTRY     = 0x40000000
cflags-y += -DPB_BOOT_TEST=1
src-y += $(BOARD)/board.c
