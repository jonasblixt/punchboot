CROSS_COMPILE ?= /opt/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-

ARCH_OUTPUT = elf64-littleaarch64
ARCH = aarch64

CFLAGS   += -march=armv8-a -DAARCH64
CFLAGS  += -mstrict-align -mgeneral-regs-only
CFLAGS   += -I arch/armv8a/include
CFLAGS   += -I include/pb/libc/aarch64

ARCH_ASM_SRCS += arch/armv8a/entry.S
ARCH_ASM_SRCS += arch/armv8a/boot.S


arch_clean:
	@-rm -rf arch/armv8a/*.o
	@-rm -f arch/armv8a/*.gcno arch/armv8a/*.gcda

