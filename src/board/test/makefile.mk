PB_BOARD_NAME = Test
PB_PLAT_NAME   = test
PB_ENTRY     = 0x40000000


CFLAGS += -I board/test/include
CFLAGS += -DPB_BOOT_TEST=1

BOARD_C_SRCS += board/test/test.c

KEYS  = ../pki/dev_rsa_public.der
KEYS += ../pki/prod_rsa_public.der
KEYS += ../pki/field1_rsa_public.der
KEYS += ../pki/field2_rsa_public.der

KEY_TYPE = RSA

board_clean:
	@-rm -rf board/test/*.o 

board_final:
