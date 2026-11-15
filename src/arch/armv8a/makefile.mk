ifdef CONFIG_ARCH_ARMV8

ARCH_OUTPUT = elf64-littleaarch64
ARCH = aarch64

cflags-y += -march=armv8-a -DAARCH64
cflags-y += -mstrict-align -mgeneral-regs-only
cflags-y += -I src/arch/armv8a/include

# TF-A's build options, which its headers here (arch.h, arch_helpers.h,
# asm_macros*.S, vendored.md) and include/xlat_tables test with #if: TF-A's
# build defines each of them, so they are defined here too (-Wundef), with
# the value an undefined one had. ARM_ARCH_MAJOR is TF-A's for AArch64. No
# erratum's workaround is built in, DEBUG and ERROR_DEPRECATED are off; only
# assembly tests the two, and DEBUG means something else to the SCFW's
# headers (include/plat/imx8x/sci/scfw.h).
cflags-y += -DARM_ARCH_MAJOR=8
cflags-y += -DERRATA_A53_819472=0 -DERRATA_A53_824069=0 -DERRATA_A53_827319=0
cflags-y += -DERRATA_A57_813419=0 -DERRATA_A76_1286807=0
asflags-y += -DDEBUG=0 -DERROR_DEPRECATED=0

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
