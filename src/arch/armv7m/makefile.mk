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

LIBS += -lgcc

src-y += src/arch/armv7m/entry.c
src-y += src/arch/armv7m/arch.c
src-y += src/arch/armv7m/boot.c
src-y += src/arch/armv7m/cache.c

endif
