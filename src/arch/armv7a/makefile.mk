ARCH_OUTPUT = elf32-littlearm
ARCH = arm

CFLAGS   += -march=armv7-a
CFLAGS   += -I arch/armv7a/include

ARCH_ASM_SRCS += arch/armv7a/entry_armv7a.S
ARCH_ASM_SRCS += arch/armv7a/arm32_aeabi_divmod_a32.S
ARCH_ASM_SRCS += arch/armv7a/uldivmod.S
ARCH_C_SRCS += arch/armv7a/arm32_aeabi_divmod.c

arch_clean:
	@-rm -rf arch/armv7a/*.o
	@-rm -f arch/armv7a/*.gcno arch/armv7a/*.gcda

