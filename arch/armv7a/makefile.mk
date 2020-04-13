ifdef CONFIG_ARCH_ARMV7

CROSS_COMPILE ?= arm-none-eabi-
ARCH_OUTPUT = elf32-littlearm
ARCH = arm

CFLAGS   += -march=armv7-a -DAARCH32
CFLAGS   += -I arch/armv7a/include
CFLAGS   += -I include/pb/libc/aarch32

LDFLAGS += -Tarch/armv7a/link.lds

ARCH_ASM_SRCS += arch/armv7a/entry_armv7a.S
ARCH_ASM_SRCS += arch/armv7a/arm32_aeabi_divmod_a32.S
ARCH_ASM_SRCS += arch/armv7a/uldivmod.S
ARCH_ASM_SRCS += arch/armv7a/boot.S

ARCH_C_SRCS += arch/armv7a/arm32_aeabi_divmod.c

endif
