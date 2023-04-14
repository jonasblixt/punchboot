src-$(CONFIG_LIB_BPAK) += src/lib/bpak.c
src-$(CONFIG_LIB_ZLIB_CRC) += src/lib/crc.c
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
src-y  += src/lib/libc/putchar.c
src-y  += src/lib/libc/assert.c

cflags-$(CONFIG_ARCH_ARMV8) += -I include/libc/aarch64
cflags-$(CONFIG_ARCH_ARMV7) += -I include/libc/aarch32

# Device tree lib
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_addresses.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_ro.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_rw.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_sw.c
src-$(CONFIG_LIB_FDT)  += src/lib/fdt/fdt_wip.c

# UUID lib
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/pack.c
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/unpack.c
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/compare.c
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/copy.c
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/unparse.c
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/parse.c
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/clear.c
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/conv.c
src-$(CONFIG_LIB_UUID)  += src/lib/uuid/isnull.c
src-$(CONFIG_LIB_UUID3)  += src/lib/uuid/uuid3.c

# VM/MMU helpers
src-$(CONFIG_LIB_XLAT_TBLS) += src/lib/xlat_tables_v2/xlat_tables_common.c
src-$(CONFIG_LIB_XLAT_TBLS_ARMV7) += src/lib/xlat_tables_v2/aarch32/xlat_tables.c
src-$(CONFIG_LIB_XLAT_TBLS_ARMV8) += src/lib/xlat_tables_v2/aarch64/xlat_tables.c
cflags-$(CONFIG_LIB_XLAT_TBLS) += -I include/xlat_tables
cflags-$(CONFIG_LIB_XLAT_TBLS_ARMV7) += -I include/xlat_tables/aarch32
cflags-$(CONFIG_LIB_XLAT_TBLS_ARMV8) += -I include/xlat_tables/aarch64
