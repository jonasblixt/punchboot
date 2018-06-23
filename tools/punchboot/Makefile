#
#  Punch BOOT bootloader cli
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


# Makefile for Punch BOOT

TARGET  = punchboot
PREFIX ?= /usr

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
OBJCOPY=$(CROSS_COMPILE)objcopy

GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)
PKG_CONFIG ?= pkg-config
CFLAGS   = -Wall -O2 
CFLAGS  += -I. -I include/ -I 3pp/libtomcrypt/src/headers
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\"
CFLAGS  += `$(PKG_CONFIG) --cflags libusb-1.0`

LIBS     = `$(PKG_CONFIG) --libs libusb-1.0` -luuid

LDFLAGS =  

ASM_SRCS = 
C_SRCS   = $(TARGET).c crc.c utils.c
C_SRCS  += recovery_protocol.c

OBJS     = $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o)

TOMCRYPT_CFLAGS  = -DARGTYPE=3 -DLTC_NO_TEST -DLTC_NO_FILE 
TOMCRYPT_CFLAGS += -DUSE_LTM -DLTM_DESC -I../libtommath/ -L../libtommath/


all: $(TARGET)

libtomcrypt:
	@$(MAKE) -C 3pp/libtommath CC=$(CC) AR=$(AR)
	@$(MAKE) -C 3pp/libtomcrypt/ CC=$(CC) AR=$(AR) \
		CFLAGS="$(TOMCRYPT_CFLAGS)" EXTRALIBS="-ltommath"


$(TARGET): $(OBJS)
	@echo LINK $@ $(LDFLAGS)
	@$(CC) $(LDFLAGS) $(CFLAGS) $(OBJS) $(LIBS) -o $@

%.o: %.S
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@
%.o: %.c
	@echo CC $< $(CFLAGS)
	@$(CC) -c $(CFLAGS) $< -o $@

install:
	@install -m 755 $(TARGET) $(PREFIX)/bin

clean: 
	@-rm -rf *.o $(TARGET)  *.map out
reallyclean: clean
	$(MAKE) -C 3pp/libtomcrypt clean
	$(MAKE) -C 3pp/libtommath clean
	$(MAKE) -C 3pp/tinyprintf clean

