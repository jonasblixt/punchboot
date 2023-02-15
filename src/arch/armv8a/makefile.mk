ifdef CONFIG_ARCH_ARMV8

ARCH_OUTPUT = elf64-littleaarch64
ARCH = aarch64

cflags-y += -march=armv8-a -DAARCH64
cflags-y += -mstrict-align -mgeneral-regs-only
cflags-y += -I src/arch/armv8a/include
cflags-y += -I src/include/pb/libc/aarch64

ldflags-y += -Tsrc/arch/armv8a/link.lds

asm-y += src/arch/armv8a/entry.S
asm-y += src/arch/armv8a/boot.S
asm-y += src/arch/armv8a/timer.S
asm-y += src/arch/armv8a/cache-ops.S
asm-y += src/arch/armv8a/misc_helpers.S

src-y += src/arch/armv8a/arch.c

endif
