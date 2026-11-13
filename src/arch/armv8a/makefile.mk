ifdef CONFIG_ARCH_ARMV8

ARCH_OUTPUT = elf64-littleaarch64
ARCH = aarch64

cflags-y += -march=armv8-a -DAARCH64
cflags-y += -mstrict-align -mgeneral-regs-only
cflags-y += -I src/arch/armv8a/include

ldflags-y += -Tsrc/arch/armv8a/link.lds
ldflags-y += --defsym=PB_SECTION_ALIGNMENT=4096
# The sections are aligned to 4 KiB, ld's aarch64 page is 64 KiB: with it, a
# .text that ends in the 64 KiB page where .data starts puts .text to .bss in
# one RWX LOAD segment (F-090). Only the ELF's headers change, not pb.bin.
ldflags-y += -z max-page-size=4096

asm-y += src/arch/armv8a/entry.S
asm-y += src/arch/armv8a/boot.S
asm-y += src/arch/armv8a/timer.S
asm-y += src/arch/armv8a/cache-ops.S
asm-y += src/arch/armv8a/misc_helpers.S

src-y += src/arch/armv8a/arch.c

endif
