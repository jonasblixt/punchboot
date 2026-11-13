# i.MX RT1060 EVK: Cortex-M7, 160 interrupts. The image runs from OCRAM2 at
# 0x20210000, between the ROM's data (0x20200000-0x20207fff, which its API
# still uses) and pb (from 0x20249000).
# pb's transfer list is in the 4 KiB above the ROM's data, 0x20208000: the
# board's CONFIG_BOOT_FH_REGION_BASE and _SIZE (configs/imxrt1060evk_defconfig),
# below what the image is linked for (link.ld).
CPU_FLAGS = -mcpu=cortex-m7 -mthumb -mfloat-abi=soft
APP_NUM_IRQS = 160
APP_LOAD_ADDR = 0x20210000
APP_FH_REGION_BASE = 0x20208000
APP_FH_REGION_SIZE = 0x1000
BOARD_SRCS = $(PB)/src/board/imxrt1060evk/fcb_is25wp064.c
