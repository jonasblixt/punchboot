
# UUID src/lib
src-y  += src/lib/uuid/pack.c
src-y  += src/lib/uuid/unpack.c
src-y  += src/lib/uuid/compare.c
src-y  += src/lib/uuid/copy.c
src-y  += src/lib/uuid/unparse.c
src-y  += src/lib/uuid/parse.c
src-y  += src/lib/uuid/clear.c
src-y  += src/lib/uuid/conv.c
src-$(CONFIG_LIB_UUID3)  += src/lib/uuid/uuid3.c

# C src/library
src-y  += src/lib/string.c
src-y  += src/lib/memmove.c
src-y  += src/lib/memchr.c
src-y  += src/lib/memcmp.c
src-y  += src/lib/strcmp.c
src-y  += src/lib/memset.c
src-y  += src/lib/strlen.c
src-y  += src/lib/printf.c
src-y  += src/lib/snprintf.c
src-y  += src/lib/strtoul.c
src-y  += src/lib/putchar.c
src-y  += src/lib/fletcher.c
src-y  += src/lib/assert.c

# src/lib fdt
src-y  += src/lib/fdt/fdt.c
src-y  += src/lib/fdt/fdt_addresses.c
src-y  += src/lib/fdt/fdt_ro.c
src-y  += src/lib/fdt/fdt_rw.c
src-y  += src/lib/fdt/fdt_sw.c
src-y  += src/lib/fdt/fdt_wip.c

# ATF vm paging src/lib

src-$(CONFIG_ARCH_ARMV7) += src/lib/xlat_tables/aarch32/xlat_tables.c
src-$(CONFIG_ARCH_ARMV8) += src/lib/xlat_tables/aarch64/xlat_tables.c
src-y += src/lib/xlat_tables/xlat_tables_common.c
cflags-y += -I src/include/pb/xlat_tables
# cflags-y += -DENABLE_ASSERTIONS
cflags-$(CONFIG_ARCH_ARMV7) += -I src/include/pb/xlat_tables/aarch32
cflags-$(CONFIG_ARCH_ARMV8) += -I src/include/pb/xlat_tables/aarch64

src-y  += src/lib/bpak.c
src-y  += src/lib/crc.c
src-y  += src/lib/gpt.c
src-y  += src/lib/asn1.c

include src/lib/bearssl/makefile.mk
