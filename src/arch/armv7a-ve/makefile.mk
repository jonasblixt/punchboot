CFLAGS   += -march=armv7ve 

CFLAGS   += -I arch/armv7a-ve 

ARCH_ASM_SRCS += arch/armv7a-ve/entry_armv7a.S
ARCH_ASM_SRCS += arch/armv7a-ve/strlen-armv7.S
ARCH_ASM_SRCS += arch/armv7a-ve/memset.S

ARCH_C_SRCS   += arch/armv7a-ve/vmsa.c

arch_clean:
	@-rm -rf arch/armv7a-ve/*.o
	@-rm -f arch/armv7a-ve/*.gcno arch/armv7a-ve/*.gcda

