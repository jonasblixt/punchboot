
# Punch BOOT Image creation tool
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
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

ifeq (,$(shell which pkg-config))
$(error "No pkg-config found")
endif

ifeq (,$(shell pkg-config --libs --cflags openssl))
$(error "No openssl detected")
endif

ifeq (,$(shell pkg-config --libs --cflags libpkcs11-helper-1))
$(error "No pkcs11-helper detected")
endif

GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)

CFLAGS  += -Wall -O2 -I ../../../include/ -std=c99
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\"
CFLAGS  += $(shell pkg-config --cflags openssl libpkcs11-helper-1)
CFLAGS  += -I . -D_POSIX_C_SOURCE
CFLAGS  += -MMD -MP

LIBS    = $(shell pkg-config --libs openssl libpkcs11-helper-1)

ASM_SRCS =
C_SRCS   = main.c
C_SRCS  += pbimage.c

$(shell mkdir -p build)

OBJS   += $(patsubst %.c, build/%.o, $(C_SRCS))
DEPS      = $(OBJS:.o=.d)

ifdef CODE_COV
	CFLAGS += -fprofile-arcs -ftest-coverage
endif

all: build/$(TARGET)

build/$(TARGET): $(OBJS)
	@echo LINK $@ $(LDFLAGS)
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
	@strip $@

build/%.o: %.c
	@mkdir -p $(@D)
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

install: all
	@install -m 755 build/$(TARGET) $(PREFIX)

clean:
	@-rm -rf build/

-include $(DEPS)
