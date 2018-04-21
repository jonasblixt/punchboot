# Makefile for Punch BOOT

TARGET  = pb

CROSS_COMPILE ?= arm-eabi-


CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
STRIP=$(CROSS_COMPILE)strip
OBJCOPY=$(CROSS_COMPILE)objcopy

GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)


ifndef BOARD
$(error BOARD is not set)
endif

RSA_KEYS = key_test

CFLAGS   = -Wall -O2 -nostdlib -nostartfiles  
CFLAGS  += -I. -I include/ -I 3pp/libtomcrypt/src/headers
CFLAGS  += -I 3pp/tinyprintf -DTINYPRINTF_OVERRIDE_LIBC=0 -DUSE_LTM -DLTM_DESC
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" -DDEBUG
LIBS     =  -ltomcrypt  -ltommath -lgcc

LD_FLAGS =
ASM_SRCS = 
C_SRCS   = main.c recovery.c crc.c gpt.c
C_SRCS  += 3pp/tinyprintf/tinyprintf.c

include board/$(BOARD)/makefile.mk

LDFLAGS += -Wl,--defsym=PB_ENTRY=$(PB_ENTRY)

$(info BOARD: [${PB_BOARD_NAME}])
$(info PLAT:  [${PB_PLAT_NAME}])

include plat/$(PB_PLAT_NAME)/makefile.mk

CFLAGS  += -I plat/$(PB_PLAT_NAME)/

$(info ARCH:  [${PB_ARCH_NAME}])

include arch/$(PB_ARCH_NAME)/makefile.mk

LDFLAGS += -Wl,-Tarch/$(PB_ARCH_NAME)/linker.lds $(CFLAGS) -Wl,--build-id=none 
LDFLAGS += -L3pp/libtomcrypt/  -L3pp/libtommath

OBJS     = $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o) key_dev.o

TOMCRYPT_CFLAGS  = -DARGTYPE=3 -DLTC_NO_TEST -DLTC_NO_FILE $(CFLAGS)
TOMCRYPT_CFLAGS += -DUSE_LTM -DLTM_DESC -I../libtommath/ -L../libtommath/

all: $(TARGET).bin

prep_keys:
	@echo KEY
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm ../pki/test_rsa_public.der key_dev.o

libtomcrypt:
	@$(MAKE) -C 3pp/libtommath CC=$(CC) AR=$(AR)
	@$(MAKE) -C 3pp/libtomcrypt/ CC=$(CC) AR=$(AR) \
		CFLAGS="$(TOMCRYPT_CFLAGS)" EXTRALIBS="-ltommath"


$(TARGET).bin: $(TARGET)
	@echo OBJCOPY $< $@
	@$(OBJCOPY) -O binary -R .comment $< $@

	@echo "VERSION = $(GIT_VERSION)"
	@$(SIZE) -A $<

$(TARGET): libtomcrypt prep_keys $(OBJS)
	@echo LINK $@
	@$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

%.o: %.S
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@
%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@


.PHONY: board_clean arch_clean plat_clean

clean: board_clean arch_clean plat_clean
	@-rm -rf *.o $(TARGET) $(TARGET).bin *.map out
reallyclean: clean
	$(MAKE) -C 3pp/libtomcrypt clean
	$(MAKE) -C 3pp/libtommath clean
	$(MAKE) -C 3pp/tinyprintf clean

