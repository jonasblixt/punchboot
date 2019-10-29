
# Punch BOOT configuration tool
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
# 
# SPDX-License-Identifier: BSD-3-Clause
# 
# 


# Makefile for Punch BOOT

TARGET  = pbconfig

PREFIX ?= /usr/bin
PKG_CONFIG ?= pkg-config
SYSROOT ?= /
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
OBJCOPY=$(CROSS_COMPILE)objcopy

ifeq (,$(shell which $(PKG_CONFIG)))
$(error "No pkg-config found")
endif

ifeq (,$(shell $(PKG_CONFIG) --libs --cflags blkid))
$(error "No blkid detected")
endif

ifeq (,$(shell $(PKG_CONFIG) --libs --cflags uuid))
$(error "No blkid detected")
endif

GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)

CFLAGS  += -Wall -O2 -I ../../include/ -std=c99
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" 
CFLAGS  += $(shell $(PKG_CONFIG) --cflags blkid uuid)
CFLAGS  += -I . --sysroot $(SYSROOT)

LIBS    = $(shell $(PKG_CONFIG) --libs blkid uuid)

ASM_SRCS = 
C_SRCS   = main.c
C_SRCS  += pbconfig.c
C_SRCS  += crc.c

OBJS     = $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o)

ifdef CODE_COV
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo LINK $@ $(LDFLAGS)
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

install: all
	@install -m 755 $(TARGET) $(PREFIX)

clean: 
	@-rm -rf $(OBJS) $(TARGET)  *.map out

