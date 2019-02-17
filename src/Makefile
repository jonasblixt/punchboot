# Makefile for Punch BOOT

TARGET  = pb


GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)

ifndef BOARD
$(error BOARD is not set)
endif

ifndef LOGLEVEL
	LOGLEVEL = 0
endif

ifdef TIMING_REPORT
	LOGLEVEL = 0
	TIMING_REPORT=1
else
	TIMING_REPORT=0
endif

CFLAGS   = -Wall -Wextra -std=c99 -Wunused-result -Os
CFLAGS  += -nostdlib -nostartfiles
CFLAGS  += -ffunction-sections 
CFLAGS  += -fdata-sections 
CFLAGS  += -fno-omit-frame-pointer
CFLAGS  += -I. -I include/pb
CFLAGS  += -I include/pb/libc
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" 
CFLAGS  += -DLOGLEVEL=$(LOGLEVEL)
CFLAGS  += -DTIMING_REPORT=$(TIMING_REPORT)
CFLAGS  += -D__PB_BUILD
CFLAGS  += -fno-common -fno-builtin
CFLAGS  += -ffreestanding -fno-exceptions
CFLAGS  += -I 3pp/fdt/include

LIBS     = 

LDFLAGS = 

ASM_SRCS = 
ARCH_ASM_SRCS =
ARCH_C_SRCS =
PLAT_ASM_SRCS =
PLAT_C_SRCS =
BLOB_INPUT  =

C_SRCS   = recovery.c
C_SRCS  += delay.c
C_SRCS  += crc.c
C_SRCS  += gpt.c 
C_SRCS  += keys.c 
C_SRCS  += timing_report.c
C_SRCS  += lib/string.c
C_SRCS  += lib/memmove.c
C_SRCS  += lib/memchr.c
C_SRCS  += lib/memcmp.c
C_SRCS  += lib/strcmp.c
C_SRCS  += lib/memset.c
C_SRCS  += lib/strlen.c
C_SRCS  += lib/printf.c
C_SRCS  += lib/snprintf.c
C_SRCS  += lib/putchar.c
C_SRCS  += lib/uuid.c
C_SRCS  += usb.c
C_SRCS  += image.c

# Lib fdt
C_SRCS  += 3pp/fdt/fdt.c
C_SRCS  += 3pp/fdt/fdt_addresses.c
C_SRCS  += 3pp/fdt/fdt_empty_tree.c
C_SRCS  += 3pp/fdt/fdt_ro.c
C_SRCS  += 3pp/fdt/fdt_rw.c
C_SRCS  += 3pp/fdt/fdt_strerror.c
C_SRCS  += 3pp/fdt/fdt_sw.c
C_SRCS  += 3pp/fdt/fdt_wip.c


RSA_KEYS_SRCS ?= ../pki/dev_rsa_public.der    \
				 ../pki/prod_rsa_public.der   \
				 ../pki/field1_rsa_public.der \
			     ../pki/field2_rsa_public.der

FINAL_IMAGE = 

include board/$(BOARD)/makefile.mk
include plat/$(PB_PLAT_NAME)/makefile.mk
include arch/$(PB_ARCH_NAME)/makefile.mk


CROSS_COMPILE ?= arm-eabi-

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
STRIP=$(CROSS_COMPILE)strip
OBJCOPY=$(CROSS_COMPILE)objcopy

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
OBJS     += $(BLOB_INPUT:.bin=.bino)

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
	@$(SIZE) -x -B $(TARGET)

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
	@$(OBJCOPY) -I binary -O $(ARCH_OUTPUT) -B $(ARCH) $< $@

%.bino: %.bin
	@echo BLOB $<
	@$(OBJCOPY) -I binary -O $(ARCH_OUTPUT) -B $(ARCH) $< $@

%.o: %.S
	@echo AS $<
	@$(CC) -D__ASSEMBLY__ -c $(CFLAGS) $< -o $@

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
	@-rm -f qemu.log
	@-rm -f tests/*.gcda tests/*.gcno
	@find -name "*.gcno" -exec rm -f {} \;
	@find -name "*.gcda" -exec rm -f {} \;

qemu:
	@$(QEMU) $(QEMU_FLAGS) -kernel $(TARGET)

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

