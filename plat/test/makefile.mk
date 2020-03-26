#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a

CFLAGS += -mtune=cortex-a15 -I plat/test/include

PLAT_C_SRCS  += plat/test/uart.c
PLAT_C_SRCS  += plat/test/usb_test.c
PLAT_C_SRCS  += plat/test/crypto.c
PLAT_C_SRCS  += plat/test/reset.c
PLAT_C_SRCS  += plat/test/pl061.c
PLAT_C_SRCS  += plat/test/semihosting.c
PLAT_C_SRCS  += plat/test/wdog.c
PLAT_C_SRCS  += plat/test/plat.c
PLAT_C_SRCS  += plat/test/gcov.c
PLAT_C_SRCS  += plat/test/virtio.c
PLAT_C_SRCS  += plat/test/virtio_serial.c
PLAT_C_SRCS  += plat/test/virtio_block.c
PLAT_C_SRCS  += plat/test/test_fuse.c
PLAT_C_SRCS  += plat/test/root_hash.c

# BearSSL

PLAT_C_SRCS  += 3pp/bearssl/sha2small.c
PLAT_C_SRCS  += 3pp/bearssl/md5.c
PLAT_C_SRCS  += 3pp/bearssl/dec32be.c
PLAT_C_SRCS  += 3pp/bearssl/dec32le.c
PLAT_C_SRCS  += 3pp/bearssl/enc32le.c
PLAT_C_SRCS  += 3pp/bearssl/enc32be.c
PLAT_C_SRCS  += 3pp/bearssl/rsa_i62_pub.c
PLAT_C_SRCS  += 3pp/bearssl/i31_decode.c
PLAT_C_SRCS  += 3pp/bearssl/i31_decmod.c
PLAT_C_SRCS  += 3pp/bearssl/i31_ninv31.c
PLAT_C_SRCS  += 3pp/bearssl/i62_modpow2.c
PLAT_C_SRCS  += 3pp/bearssl/i31_encode.c
PLAT_C_SRCS  += 3pp/bearssl/i31_bitlen.c
PLAT_C_SRCS  += 3pp/bearssl/i31_modpow2.c
PLAT_C_SRCS  += 3pp/bearssl/i31_tmont.c
PLAT_C_SRCS  += 3pp/bearssl/i31_muladd.c
PLAT_C_SRCS  += 3pp/bearssl/i32_div32.c
PLAT_C_SRCS  += 3pp/bearssl/i31_sub.c
PLAT_C_SRCS  += 3pp/bearssl/i31_add.c
PLAT_C_SRCS  += 3pp/bearssl/i31_montmul.c
PLAT_C_SRCS  += 3pp/bearssl/i31_fmont.c
PLAT_C_SRCS  += 3pp/bearssl/ccopy.c

PLAT_ASM_SRCS += plat/test/semihosting_call.S

CFLAGS += -fprofile-arcs -ftest-coverage

