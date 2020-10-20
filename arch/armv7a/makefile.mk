ifdef CONFIG_ARCH_ARMV7

ARCH_OUTPUT = elf32-littlearm
ARCH = arm

cflags-y += -march=armv7-a
cflags-y += -mfloat-abi=soft
cflags-y += -DAARCH32
cflags-y += -I arch/armv7a/include
cflags-y += -I include/pb/libc/aarch32
cflags-y += -mno-unaligned-access
cflags-y += -DARM_ARCH_MAJOR=7
cflags-y += -DARMV7_SUPPORTS_LARGE_PAGE_ADDRESSING

ldflags-y += -Tarch/armv7a/link.lds

asm-y += arch/armv7a/entry_armv7a.S
asm-y += arch/armv7a/arm32_aeabi_divmod_a32.S
asm-y += arch/armv7a/uldivmod.S
asm-y += arch/armv7a/boot.S
asm-y += arch/armv7a/timer.S
asm-y += arch/armv7a/cp15.S
asm-y += arch/armv7a/misc_helpers.S

src-y += arch/armv7a/arm32_aeabi_divmod.c
src-y += arch/armv7a/arch.c

endif
