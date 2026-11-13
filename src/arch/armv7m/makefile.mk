ifdef CONFIG_ARCH_ARMV7M

ARCH_OUTPUT = elf32-littlearm
ARCH = arm

ARCHFLAGS := -mcpu=cortex-m7 -mfloat-abi=soft -mthumb

cflags-y += $(ARCHFLAGS)
cflags-y += -DAARCH32
cflags-y += -I src/arch/armv7m/include
cflags-y += -I include/libc/aarch32

ldflags-y += -Tsrc/arch/armv7m/link.lds
ldflags-y += -L${shell dirname "`$(CROSS_COMPILE)gcc $(ARCHFLAGS) --print-libgcc-file-name`"}
ldflags-y += --defsym=PB_SECTION_ALIGNMENT=8
# The sections are aligned to 8 bytes and .data follows .text in the same
# 64 KiB page, ld's arm page: ld put .text to .stack in one RWX LOAD segment
# (F-396). With 4 byte pages .text is an R E segment and .data an RW one.
# The Cortex-M7 has no MMU; only the ELF's headers change, not pb.bin.
ldflags-y += -z max-page-size=4

LIBS += -lgcc

src-y += src/arch/armv7m/entry.c
src-y += src/arch/armv7m/arch.c
src-y += src/arch/armv7m/boot.c
src-y += src/arch/armv7m/cache.c
asm-y += src/arch/armv7m/mmu.S

endif
