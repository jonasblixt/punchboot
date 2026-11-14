src-y += $(BOARD)/board.c

# The board's memory (platform.md 5.1): where pb is linked, pb's region,
# the DRAM and pb's buffers. The link checks them against the SoC.
# pb runs in the DRAM, 1 MiB of it, with an image data region between pb
# and its buffers (board.c). The buffers end at 0x82000000, where the kernel
# of the images in use is: the platform's 16 MiB would reach into it. The
# base is the platform's too, set here because the end is the board's.
PB_ENTRY        = 0x80001000
PB_SIZE         = 0x100000
BOARD_DRAM_BASE = 0x80000000
BOARD_DRAM_SIZE = 0x40000000
PB_BUFFERS_BASE = 0x81021000
PB_BUFFERS_SIZE = 0xfdf000

# The image stages are the i.MX6UL's (src/plat/imx6ul/makefile.mk), turned on
# and given the keys in the defconfig. Their input from the board is its DCD,
# imximage.cfg, which they find here.
