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

ifndef LOGLEVEL
	LOGLEVEL = 0
endif

SYS_ROOT_PATH = $(shell ${CC} -print-sysroot)

CFLAGS   = -Wall -Wextra -Wunused-result -pedantic-errors -O2
CFLAGS  += -nostdlib -nostartfiles
CFLAGS  += -Wimplicit-fallthrough=0
CFLAGS  += -I. -I include/
CFLAGS  += -DTINYPRINTF_OVERRIDE_LIBC=0
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" 
CFLAGS  += -DLOGLEVEL=$(LOGLEVEL)
CFLAGS  += -isystem $(SYS_ROOT_PATH)/usr/lib/include
CFLAGS  += --specs=nosys.specs -fno-common -fno-builtin
CFLAGS  += -ffreestanding -fno-exceptions

LIBS     = 

LDFLAGS =

ASM_SRCS = 
ARCH_ASM_SRCS =
ARCH_C_SRCS =
PLAT_ASM_SRCS =
PLAT_C_SRCS =

C_SRCS   = main.c
C_SRCS  += recovery.c
C_SRCS  += crc.c
C_SRCS  += gpt.c 
C_SRCS  += keys.c 
C_SRCS  += config.c
C_SRCS  += lib/string.c
C_SRCS  += recovery_protocol.c
C_SRCS  += usb.c
C_SRCS  += tinyprintf.c
C_SRCS  += pb_image.c

RSA_KEYS_SRCS ?= ../pki/dev_rsa_public.der    \
				 ../pki/prod_rsa_public.der   \
				 ../pki/field1_rsa_public.der \
			     ../pki/field2_rsa_public.der

FINAL_IMAGE = 

include board/$(BOARD)/makefile.mk
include plat/$(PB_PLAT_NAME)/makefile.mk
include arch/$(PB_ARCH_NAME)/makefile.mk

LDFLAGS += --defsym=PB_ENTRY=$(PB_ENTRY) 
LDFLAGS += -Tlink.lds -Tarch/$(PB_ARCH_NAME)/link.lds --build-id=none 

ARCH_OBJS     = $(ARCH_ASM_SRCS:.S=.o) $(ARCH_C_SRCS:.c=.o) 
PLAT_OBJS     = $(PLAT_ASM_SRCS:.S=.o) $(PLAT_C_SRCS:.c=.o) 
BOARD_OBJS    = $(BOARD_ASM_SRCS:.S=.o) $(BOARD_C_SRCS:.c=.o) 

OBJS	  = $(ARCH_OBJS) $(PLAT_OBJS) $(BOARD_OBJS)  
OBJS     += $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o) 
OBJS	 += $(RSA_KEYS_SRCS:.der=.dero)

TOMCRYPT_CFLAGS  = -DARGTYPE=3 -DLTC_NO_TEST -DLTC_NO_FILE $(CFLAGS)
TOMCRYPT_CFLAGS += -DUSE_LTM -DLTM_DESC -I../libtommath/ -L../libtommath/

ifeq ($(BOARD),test)
	include tests/makefile.mk
endif

all: $(TARGET).bin board_final
	$(info Summary:)
	$(info )
	$(info BOARD:     [${PB_BOARD_NAME}])
	$(info PLAT:      [${PB_PLAT_NAME}])
	$(info ARCH:      [${PB_ARCH_NAME}])
	$(info LOGLEVEL:  [${LOGLEVEL}])
	@echo "VERSION = $(GIT_VERSION)"
	$(info )
	@$(SIZE) -A $(TARGET)

todo:
	@grep -lR "TODO:" *

libtomcrypt:
	@$(MAKE) -C 3pp/libtommath CC=$(CC) AR=$(AR)
	@$(MAKE) -C 3pp/libtomcrypt/ CC=$(CC) AR=$(AR) \
		CFLAGS="$(TOMCRYPT_CFLAGS)" EXTRALIBS="-ltommath"

$(TARGET).bin: $(TARGET)
	@echo OBJCOPY $< $@
	@$(OBJCOPY) -O binary -R .comment $< $@

$(TARGET): $(OBJS)
	@echo LD $@
	@$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

%.dero: %.der
	@echo KEY $<
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm $< $@

%.o: %.S
	@echo AS $<
	@$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

install: board_final
	@punchboot boot -w -r -f $(FINAL_IMAGE)

.PHONY: board_clean arch_clean plat_clean

clean: board_clean arch_clean plat_clean
	@-rm -rf *.o $(TARGET) $(TARGET).bin *.map out
	@-rm -rf tests/*.o
	@-rm -f $(OBJS)
	@-rm -f *.gcda *.gcno
	@-rm -f tests/*.gcda tests/*.gcno
	@find -name "*.gcno" -exec rm -f {} \;
	@find -name "*.gcda" -exec rm -f {} \;



reallyclean: clean
	$(MAKE) -C 3pp/libtomcrypt clean
	$(MAKE) -C 3pp/libtommath clean

.DEFAULT_GOAL := all

# Dependencies

boot.o:     include/pb.h
config.o:   include/config.h include/pb.h
crc.o:      include/pb.h include/crc.h
gpt.o:		include/pb.h include/gpt.h
keys.o:		include/pb.h include/keys.h
main.o:		include/pb.h
usb.o:		include/pb.h include/usb.h
recovery.o: include/recovery.h include/pb.h

