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

CC ?= $(CROSS_COMPILE)gcc
LD ?= $(CROSS_COMPILE)ld
AR ?= $(CROSS_COMPILE)ar
SIZE ?= $(CROSS_COMPILE)size
OBJCOPY ?= $(CROSS_COMPILE)objcopy

SYSROOT ?= $(shell $(CC) -print-sysroot)

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

CFLAGS  += -Wall -O2 -I ../../../include/ -std=c99
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" 
CFLAGS  += $(shell $(PKG_CONFIG) --cflags blkid uuid)
CFLAGS  += -I . --sysroot $(SYSROOT)
CFLAGS  += -MMD -MP

LIBS    = $(shell $(PKG_CONFIG) --libs blkid uuid)

C_SRCS   = main.c
C_SRCS  += pbconfig.c
C_SRCS  += crc.c

$(shell mkdir -p build)

OBJS   += $(patsubst %.c, build/%.o, $(C_SRCS))
DEPS	= $(OBJS:.o=.d)

ifdef CODE_COV
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

all: build/$(TARGET)

build/$(TARGET): $(OBJS)
	@echo LINK $@ $(LDFLAGS)
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

build/%.o: %.c
	@mkdir -p $(@D)
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

install: all
	@install -m 755 $(TARGET) $(PREFIX)

clean: 
	@-rm -rf build/

-include $(DEPS)

