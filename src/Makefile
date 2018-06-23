# Makefile for Punch BOOT

TARGET  = pb

CROSS_COMPILE ?= arm-eabi-


CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
SIZE=$(CROSS_COMPILE)size
STRIP=$(CROSS_COMPILE)strip
OBJCOPY=$(CROSS_COMPILE)objcopy
UFSMIMPORT ?= ufsmimport
GIT_VERSION = $(shell git describe --abbrev=4 --dirty --always --tags)

ifndef BOARD
$(error BOARD is not set)
endif

CFLAGS   = -Wall -Wextra -Wunused-result -pedantic-errors -O2 -nostdlib -nostartfiles  
CFLAGS  += -Wimplicit-fallthrough=0
CFLAGS  += -I. -I include/
CFLAGS  += -DTINYPRINTF_OVERRIDE_LIBC=0
CFLAGS  += -DVERSION=\"$(GIT_VERSION)\" 
CFLAGS  += -I 3pp/ufsm/src
CFLAGS  += -DLOGLEVEL=$(LOGLEVEL)
LIBS     = 

LD_FLAGS =
ASM_SRCS = 
C_SRCS   = main.c 
C_SRCS  += recovery.c
C_SRCS  += crc.c
C_SRCS  += gpt.c 
C_SRCS  += keys.c 
C_SRCS  += boot.c
C_SRCS  += config.c
C_SRCS  += pb_string.c
C_SRCS  += recovery_protocol.c
C_SRCS  += usb.c
C_SRCS  += tinyprintf.c
C_SRCS  += 3pp/ufsm/src/ufsm.c
C_SRCS  += 3pp/ufsm/src/ufsm_stack.c
C_SRCS  += 3pp/ufsm/src/ufsm_queue.c


RSA_KEYS_SRCS ?= ../pki/dev_rsa_public.der    \
				 ../pki/prod_rsa_public.der   \
				 ../pki/field1_rsa_public.der \
			     ../pki/field2_rsa_public.der

UFSM_SRCS = pb_fsm.xmi

FINAL_IMAGE = 

include board/$(BOARD)/makefile.mk
include plat/$(PB_PLAT_NAME)/makefile.mk
include arch/$(PB_ARCH_NAME)/makefile.mk

LDFLAGS += -Wl,--defsym=PB_ENTRY=$(PB_ENTRY)
LDFLAGS += -Wl,-Tarch/$(PB_ARCH_NAME)/linker.lds $(CFLAGS) -Wl,--build-id=none 

OBJS      = $(ASM_SRCS:.S=.o) $(C_SRCS:.c=.o) 
OBJS     += $(UFSM_SRCS:.xmi=.xmio)
OBJS	 += $(RSA_KEYS_SRCS:.der=.dero)

TOMCRYPT_CFLAGS  = -DARGTYPE=3 -DLTC_NO_TEST -DLTC_NO_FILE $(CFLAGS)
TOMCRYPT_CFLAGS += -DUSE_LTM -DLTM_DESC -I../libtommath/ -L../libtommath/

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
	@echo LINK $@
	@$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

%.xmio: %.xmi
	@echo UFSMIMPORT $<
	@$(UFSMIMPORT) $< $(patsubst %.xmi, %, $(<))
	@$(CC) $(CFLAGS) -c $(patsubst %.xmi, %.c, $(<)) -o $(patsubst %.xmi, %.xmio, $(<))

%.dero: %.der
	@echo KEY $<
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm $< $@

%.o: %.S
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

install: board_final
	@punchboot boot -w -r -f $(FINAL_IMAGE)

.PHONY: board_clean arch_clean plat_clean

clean: board_clean arch_clean plat_clean
	@-rm -rf *.o $(TARGET) $(TARGET).bin *.map out
	@-rm -f $(OBJS)

reallyclean: clean
	$(MAKE) -C 3pp/libtomcrypt clean
	$(MAKE) -C 3pp/libtommath clean
	$(MAKE) -C 3pp/ufsm/src clean

.DEFAULT_GOAL := all
