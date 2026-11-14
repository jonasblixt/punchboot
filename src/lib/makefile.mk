src-y  += src/lib/bpak.c
src-y  += src/lib/crc.c
src-$(CONFIG_LIB_DER_HELPERS) += src/lib/der_helpers.c

# C standard library functions
src-y  += src/lib/libc/string.c
src-y  += src/lib/libc/memmove.c
src-y  += src/lib/libc/memchr.c
src-y  += src/lib/libc/memcmp.c
src-y  += src/lib/libc/strcmp.c
src-y  += src/lib/libc/memset.c
src-y  += src/lib/libc/strlen.c
src-y  += src/lib/libc/printf.c
src-y  += src/lib/libc/snprintf.c
src-y  += src/lib/libc/strtoul.c
src-y  += src/lib/libc/strchr.c
src-y  += src/lib/libc/strcspn.c
src-y  += src/lib/libc/strspn.c
src-y  += src/lib/libc/strcpy.c
src-y  += src/lib/libc/putchar.c
src-y  += src/lib/libc/assert.c

cflags-$(CONFIG_ARCH_ARMV8) += -I include/libc/aarch64
cflags-$(CONFIG_ARCH_ARMV7) += -I include/libc/aarch32
# TF-A's build option, which the libc's cdefs.h tests with #if: TF-A's build
# defines it, so it is defined here too (-Wundef), off, as it was undefined
cflags-y += -DRECLAIM_INIT_CODE=0

# Device tree lib
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_addresses.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_ro.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_rw.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_sw.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_wip.c

cflags-$(CONFIG_LIB_FDT) += -I include/fdt

# UUID lib
src-y  += src/lib/uuid/pack.c
src-y  += src/lib/uuid/unpack.c
src-y  += src/lib/uuid/compare.c
src-y  += src/lib/uuid/copy.c
src-y  += src/lib/uuid/unparse.c
src-y  += src/lib/uuid/parse.c
src-y  += src/lib/uuid/clear.c
src-y  += src/lib/uuid/conv.c
src-y  += src/lib/uuid/isnull.c
src-y  += src/lib/uuid/uuid3.c

# VM/MMU helpers
src-$(CONFIG_LIB_XLAT_TBLS) += src/lib/xlat_tables_v2/xlat_tables_common.c
# TF-A's (vendored.md) tests MMU_LOG_LEVEL, a switch of its own for the debug
# print that nothing defines, and ENABLE_ASSERTIONS: -Wundef is off for this
# object only, which keeps both undefined, as they were
$(BUILD_DIR)/src/lib/xlat_tables_v2/xlat_tables_common.o: cflags-y += -Wno-undef
src-$(CONFIG_LIB_XLAT_TBLS_ARMV7) += src/lib/xlat_tables_v2/aarch32/xlat_tables.c
src-$(CONFIG_LIB_XLAT_TBLS_ARMV8) += src/lib/xlat_tables_v2/aarch64/xlat_tables.c
cflags-$(CONFIG_LIB_XLAT_TBLS) += -I include/xlat_tables
cflags-$(CONFIG_LIB_XLAT_TBLS_ARMV7) += -I include/xlat_tables/aarch32
cflags-$(CONFIG_LIB_XLAT_TBLS_ARMV8) += -I include/xlat_tables/aarch64
# Their room: Kconfig values, with the platform's default (platform.md 5.1)
ifdef CONFIG_LIB_XLAT_TBLS
$(if $(CONFIG_MAX_MMAP_REGIONS),,$(error CONFIG_MAX_MMAP_REGIONS: the platform gives no default (platform.md 5.1)))
$(if $(CONFIG_MAX_XLAT_TABLES),,$(error CONFIG_MAX_XLAT_TABLES: the platform gives no default (platform.md 5.1)))
cflags-y += -DMAX_MMAP_REGIONS=$(CONFIG_MAX_MMAP_REGIONS)
cflags-y += -DMAX_XLAT_TABLES=$(CONFIG_MAX_XLAT_TABLES)
endif
