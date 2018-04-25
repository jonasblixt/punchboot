
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
CFLAGS   = -Wall -O2 
CFLAGS  += -I. -DUSE_LTM -DLTM_DESC 
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" 

LIBS    =   -luuid -ltomcrypt -ltommath
LDFLAGS =

ASM_SRCS = 
C_SRCS   = $(TARGET).c

OBJS     = $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o)


all: $(TARGET)

$(TARGET): $(OBJS)
	@echo LINK $@ $(LDFLAGS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
	@strip $@

%.o: %.S
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@
%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

install: all
	@install -m 755 $(TARGET) $(PREFIX)

clean: 
	@-rm -rf *.o $(TARGET)  *.map out

