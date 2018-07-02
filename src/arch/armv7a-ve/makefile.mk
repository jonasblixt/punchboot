CFLAGS   += -march=armv7ve 

CFLAGS   += -I arch/armv7a-ve 

ASM_SRCS += arch/armv7a-ve/entry_armv7a.S
ASM_SRCS += arch/armv7a-ve/strlen-armv7.S
ASM_SRCS += arch/armv7a-ve/memset.S

C_SRCS   += arch/armv7a-ve/vmsa.c

arch_clean:
	@-rm -rf arch/armv7a-ve/*.o
