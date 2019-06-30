PB_BOARD_NAME = Bebop
PB_PLAT_NAME   = imx6ul
PB_ENTRY     = 0x80001000

BOARD_C_SRCS += board/bebop/board.c
BOARD_C_SRCS += boot/atf_linux_initrd.c

FINAL_IMAGE     = $(TARGET).imx

KEYS  = ../pki/secp256r1-pub-key.der

KEY_TYPE = EC

