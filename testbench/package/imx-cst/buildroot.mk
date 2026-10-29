# CST toolchain file (TOOLCHAIN=buildroot), replaces gcc.mk which
# hardcodes gcc and -Werror. Based on Debian's debian-toolchain.mk.

LD = $(CC)

COPTIONS += -std=c99 -D_POSIX_C_SOURCE=200809L -fPIC $(BR_CFLAGS)

LDOPTIONS += $(BR_LDFLAGS)

LDLIBS := -lcrypto -ldl

ARFLAGS := -rc
