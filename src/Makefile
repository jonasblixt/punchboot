# Makefile

PACKAGE:=pb

CROSS_COMPILE ?= arm-eabi-

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
AR=$(CROSS_COMPILE)ar
OBJCOPY=$(CROSS_COMPILE)objcopy

CFLAGS  = -g -Os -nostartfiles -march=armv7ve -mtune=cortex-a7 
CFLAGS += -DTEXT_START=0x81000000
CFLAGS += -I. -I 3pp/libtomcrypt/src/headers 

LDFLAGS  = -Wl,-T$(PACKAGE).lds $(CFLAGS) -Wl,--build-id=none
LDFLAGS += -L3pp/libtomcrypt/ 

LIBS =  -ltomcrypt 

SRCS:= start.S
CSRCS:= pb.c mini-printf.c
OBJS:=$(SRCS:.S=.o)
COBJS:=$(CSRCS:.c=.o)

TOMCRYPT_CFLAGS = -DUSE_LTM -DLTM_DESC -I../libtommath/ -L../libtommath/

all: libtomcrypt $(PACKAGE).bin

libtomcrypt:
	$(MAKE) -C 3pp/libtommath CC=$(CC) AR=$(AR)
	$(MAKE) -C 3pp/libtomcrypt/ CC=$(CC) AR=$(AR) CFLAGS="$(TOMCRYPT_CFLAGS)" EXTRALIBS="-ltommath"

$(PACKAGE).bin: $(PACKAGE)
	$(OBJCOPY) -O binary -R .comment $< $@
	@mkimage -n imximage.cfg -T imximage -a 0x81000000 -e 0x81000000 -d $(PACKAGE).bin $(PACKAGE).imx

$(PACKAGE): libtomcrypt $(OBJS) $(COBJS)
	$(CC) $(LDFLAGS) $(OBJS)  $(COBJS) $(LIBS) -o $@

%.o: %.S
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
.PHONY: clean distclean

clean:
	@-rm -rf *.o $(PACKAGE) $(PACKAGE).bin *.map out
	$(MAKE) -C 3pp/libtomcrypt clean
	$(MAKE) -C 3pp/libtommath clean
	$(MAKE) -C 3pp/musl clean

prog:
	@imx_usb ./pb.imx
