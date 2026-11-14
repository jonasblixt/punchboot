ifdef CONFIG_ARCH_ARMV7

ARCH_OUTPUT = elf32-littlearm
ARCH = arm

cflags-y += -march=armv7-a
cflags-y += -mfloat-abi=soft
cflags-y += -DAARCH32
cflags-y += -I src/arch/armv7a/include
cflags-y += -mno-unaligned-access
cflags-y += -DARM_ARCH_MAJOR=7
cflags-y += -DARMV7_SUPPORTS_LARGE_PAGE_ADDRESSING

ldflags-y += -Tsrc/arch/armv7a/link.lds
ldflags-y += --defsym=PB_SECTION_ALIGNMENT=4096
# The sections are aligned to 4 KiB, ld's arm page is 64 KiB: with it, .text
# and .data share a page and ld puts .text to .bss in one RWX LOAD segment
# (F-396, F-090 on armv8a). Only the ELF's headers change, not pb.bin.
ldflags-y += -z max-page-size=4096

asm-y += src/arch/armv7a/entry_armv7a.S
asm-y += src/arch/armv7a/arm32_aeabi_divmod_a32.S
asm-y += src/arch/armv7a/uldivmod.S
asm-y += src/arch/armv7a/boot.S
asm-y += src/arch/armv7a/timer.S
asm-y += src/arch/armv7a/cp15.S
asm-y += src/arch/armv7a/misc_helpers.S

src-y += src/arch/armv7a/arm32_aeabi_divmod.c
src-y += src/arch/armv7a/arch.c

endif
