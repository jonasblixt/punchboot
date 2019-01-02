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
CFLAGS  += -I. -I include/pb
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

C_SRCS   = boot.c
C_SRCS  += recovery.c
C_SRCS  += crc.c
C_SRCS  += gpt.c 
C_SRCS  += keys.c 
C_SRCS  += config.c
C_SRCS  += lib/string.c
C_SRCS  += usb.c
C_SRCS  += tinyprintf.c
C_SRCS  += image.c

RSA_KEYS_SRCS ?= ../pki/dev_rsa_public.der    \
				 ../pki/prod_rsa_public.der   \
				 ../pki/field1_rsa_public.der \
			     ../pki/field2_rsa_public.der

FINAL_IMAGE = 

include board/$(BOARD)/makefile.mk
include plat/$(PB_PLAT_NAME)/makefile.mk
include arch/$(PB_ARCH_NAME)/makefile.mk

LDFLAGS += --defsym=PB_ENTRY=$(PB_ENTRY) 
LDFLAGS += -Tarch/$(PB_ARCH_NAME)/link.lds
LDFLAGS += -Tplat/$(PB_PLAT_NAME)/link.lds
LDFLAGS += -Tlink.lds  --build-id=none 

ARCH_OBJS     = $(ARCH_ASM_SRCS:.S=.o) $(ARCH_C_SRCS:.c=.o) 
PLAT_OBJS     = $(PLAT_ASM_SRCS:.S=.o) $(PLAT_C_SRCS:.c=.o) 
BOARD_OBJS    = $(BOARD_ASM_SRCS:.S=.o) $(BOARD_C_SRCS:.c=.o) 

OBJS	  = $(ARCH_OBJS) $(PLAT_OBJS) $(BOARD_OBJS)  
OBJS     += $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o) 
OBJS	 += $(RSA_KEYS_SRCS:.der=.dero)

ifeq ($(BOARD),test)
	include tests/makefile.mk
endif

all: plat_early $(TARGET).bin board_final plat_final
	$(info Summary:)
	$(info )
	$(info BOARD:     [${PB_BOARD_NAME}])
	$(info PLAT:      [${PB_PLAT_NAME}])
	$(info ARCH:      [${PB_ARCH_NAME}])
	$(info LOGLEVEL:  [${LOGLEVEL}])
	@echo "VERSION = $(GIT_VERSION)"
	$(info )
	@$(SIZE) -x -A $(TARGET)

todo:
	@grep -lR "TODO:" *

$(TARGET).bin: $(TARGET)
	@echo OBJCOPY $< $@
	@$(OBJCOPY) -O binary -R .comment $< $@

$(TARGET): $(OBJS) main.o
	@echo LD $@
	@$(LD) $(LDFLAGS) $(OBJS) main.o $(LIBS) -o $@

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

.PHONY: board_clean arch_clean plat_clean plat_early plat_final

clean: board_clean arch_clean plat_clean
	@-rm -rf *.o $(TARGET) $(TARGET).bin *.map out
	@-rm -rf tests/*.o
	@-rm -f $(OBJS)
	@-rm -f *.gcda *.gcno
	@-rm -f tests/*.gcda tests/*.gcno
	@find -name "*.gcno" -exec rm -f {} \;
	@find -name "*.gcda" -exec rm -f {} \;

qemu:
	@$(QEMU) $(QEMU_FLAGS) -kernel $(TARGET)

reallyclean: clean
	$(MAKE) -C 3pp/libtomcrypt clean
	$(MAKE) -C 3pp/libtommath clean

.DEFAULT_GOAL := all

# Dependencies
# TODO: Review Dependencies

boot.o:     include/pb/pb.h
config.o:   include/pb/config.h include/pb/pb.h
crc.o:      include/pb/pb.h include/pb/crc.h
gpt.o:		include/pb/pb.h include/pb/gpt.h
keys.o:		include/pb/pb.h include/pb/keys.h
main.o:		include/pb/pb.h
usb.o:		include/pb/pb.h include/pb/usb.h
recovery.o: include/pb/recovery.h include/pb/pb.h

