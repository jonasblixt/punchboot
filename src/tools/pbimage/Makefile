
# Punch BOOT Image creation tool
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
# 
# SPDX-License-Identifier: BSD-3-Clause
# 
# 


# Makefile for Punch BOOT

TARGET  = pbimage

PREFIX ?= /usr/bin

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
OBJCOPY=$(CROSS_COMPILE)objcopy

GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)
PKG_CONFIG ?= pkg-config
CFLAGS   = -Wall -O2 -I ../../include/ 
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" 
CFLAGS  += -std=c99
CFLAGS  += -I .

LIBS    =   -lc -luuid
LDFLAGS =

ASM_SRCS = 
C_SRCS   = main.c
C_SRCS  += pbimage.c

# BearSSL

C_SRCS  += 3pp/bearssl/sha2small.c
C_SRCS  += 3pp/bearssl/dec32be.c
C_SRCS  += 3pp/bearssl/enc32be.c
C_SRCS  += 3pp/bearssl/x509_decoder.c
C_SRCS  += 3pp/bearssl/skey_decoder.c
C_SRCS  += 3pp/bearssl/rsa_i62_pkcs1_vrfy.c
C_SRCS  += 3pp/bearssl/rsa_i62_pkcs1_sign.c
C_SRCS  += 3pp/bearssl/rsa_pkcs1_sig_unpad.c
C_SRCS  += 3pp/bearssl/rsa_pkcs1_sig_pad.c
C_SRCS  += 3pp/bearssl/rsa_i62_pub.c
C_SRCS  += 3pp/bearssl/rsa_i62_priv.c
C_SRCS  += 3pp/bearssl/i31_decode.c
C_SRCS  += 3pp/bearssl/i31_decmod.c
C_SRCS  += 3pp/bearssl/i31_mulacc.c
C_SRCS  += 3pp/bearssl/i31_reduce.c
C_SRCS  += 3pp/bearssl/i31_decred.c
C_SRCS  += 3pp/bearssl/i31_rshift.c
C_SRCS  += 3pp/bearssl/i31_ninv31.c
C_SRCS  += 3pp/bearssl/i62_modpow2.c
C_SRCS  += 3pp/bearssl/i31_encode.c
C_SRCS  += 3pp/bearssl/i31_bitlen.c
C_SRCS  += 3pp/bearssl/i31_modpow2.c
C_SRCS  += 3pp/bearssl/i31_tmont.c
C_SRCS  += 3pp/bearssl/i31_muladd.c
C_SRCS  += 3pp/bearssl/i32_div32.c
C_SRCS  += 3pp/bearssl/i31_sub.c
C_SRCS  += 3pp/bearssl/i31_add.c
C_SRCS  += 3pp/bearssl/i31_montmul.c
C_SRCS  += 3pp/bearssl/i31_fmont.c
C_SRCS  += 3pp/bearssl/i31_modpow.c
C_SRCS  += 3pp/bearssl/ccopy.c

OBJS     = $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o)

ifdef CODE_COV
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo LINK $@ $(LDFLAGS)
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
	@strip $@

%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

install: all
	@install -m 755 $(TARGET) $(PREFIX)

clean: 
	@-rm -rf $(OBJS) $(TARGET)  *.map out

