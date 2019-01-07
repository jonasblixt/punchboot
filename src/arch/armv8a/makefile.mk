CROSS_COMPILE = aarch64-elf-

CFLAGS   += -march=armv8-a
CFLAGS   += -I arch/armv8a/include

ARCH_ASM_SRCS += arch/armv8a/entry.S

arch_clean:
	@-rm -rf arch/armv8a/*.o
	@-rm -f arch/armv8a/*.gcno arch/armv8a/*.gcda

