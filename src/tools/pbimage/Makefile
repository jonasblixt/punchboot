
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

CFLAGS  += -Wall -O2 -I ../../include/ 
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" 
CFLAGS  += $(shell pkg-config --cflags openssl libpkcs11-helper-1)
CFLAGS  += -I .

LIBS    = $(shell pkg-config --libs openssl libpkcs11-helper-1)

ASM_SRCS = 
C_SRCS   = main.c
C_SRCS  += pbimage.c

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

