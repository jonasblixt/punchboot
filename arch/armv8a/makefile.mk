CROSS_COMPILE ?= aarch64-linux-gnu-

ARCH_OUTPUT = elf64-littleaarch64
ARCH = aarch64

CFLAGS   += -march=armv8-a -DAARCH64
CFLAGS  += -mstrict-align -mgeneral-regs-only
CFLAGS   += -I arch/armv8a/include
CFLAGS   += -I include/pb/libc/aarch64

ARCH_ASM_SRCS += arch/armv8a/entry.S
ARCH_ASM_SRCS += arch/armv8a/boot.S

