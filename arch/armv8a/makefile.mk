ifdef CONFIG_ARCH_ARMV8

ARCH_OUTPUT = elf64-littleaarch64
ARCH = aarch64

cflags-y += -march=armv8-a -DAARCH64
cflags-y += -mstrict-align -mgeneral-regs-only
cflags-y += -I arch/armv8a/include
cflags-y += -I include/pb/libc/aarch64

ldflags-y += -Tarch/armv8a/link.lds

asm-y += arch/armv8a/entry.S
asm-y += arch/armv8a/boot.S
asm-y += arch/armv8a/timer.S
asm-y += arch/armv8a/cache-ops.S

src-y += arch/armv8a/arch.c

endif
