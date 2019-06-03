PB_BOARD_NAME = Jiffy
PB_PLAT_NAME   = imx6ul
PB_ENTRY     = 0x80001000

BOARD_C_SRCS += board/jiffy/jiffy.c
BOARD_C_SRCS += boot/atf_linux.c

KEYS  = ../pki/secp256r1-pub-key.der

KEY_TYPE = EC

