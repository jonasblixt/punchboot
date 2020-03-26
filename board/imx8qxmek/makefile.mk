PB_BOARD_NAME = imx8qxmek
PB_PLAT_NAME   = imx8x
PB_ENTRY     = 0x80000000

CFLAGS += -I board/imx8qxmek/include

BOARD_C_SRCS += board/imx8qxmek/board.c

MKIMAGE         = mkimage_imx8
FINAL_IMAGE     = $(TARGET).imx

KEYS  = ../pki/secp256r1-pub-key.der
KEYS += ../pki/secp384r1-pub-key.der
KEYS += ../pki/secp521r1-pub-key.der
#KEYS += ../pki/field1_rsa_public.der
#KEYS += ../pki/field2_rsa_public.der

KEY_TYPE = EC

