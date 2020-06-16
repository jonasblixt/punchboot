
# UUID lib
src-y  += lib/uuid/pack.c
src-y  += lib/uuid/unpack.c
src-y  += lib/uuid/compare.c
src-y  += lib/uuid/copy.c
src-y  += lib/uuid/unparse.c
src-y  += lib/uuid/parse.c
src-y  += lib/uuid/clear.c
src-y  += lib/uuid/conv.c
src-y  += lib/uuid/uuid3.c

# C library
src-y  += lib/string.c
src-y  += lib/memmove.c
src-y  += lib/memchr.c
src-y  += lib/memcmp.c
src-y  += lib/strcmp.c
src-y  += lib/memset.c
src-y  += lib/strlen.c
src-y  += lib/printf.c
src-y  += lib/snprintf.c
src-y  += lib/strtoul.c
src-y  += lib/putchar.c
src-y  += lib/fletcher.c

# Lib fdt
src-y  += lib/fdt/fdt.c
src-y  += lib/fdt/fdt_addresses.c
src-y  += lib/fdt/fdt_ro.c
src-y  += lib/fdt/fdt_rw.c
src-y  += lib/fdt/fdt_sw.c
src-y  += lib/fdt/fdt_wip.c

src-y  += lib/bpak.c
src-y  += lib/crc.c
src-y  += lib/gpt.c
src-y  += lib/asn1.c

include lib/bearssl/makefile.mk
