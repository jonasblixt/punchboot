# Makefile

PACKAGE:=pb

CROSS_COMPILE ?= arm-linux-gnueabihf-

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
OBJCOPY=$(CROSS_COMPILE)objcopy

CFLAGS=-g -Os -nostartfiles -nostdlib -march=armv7ve -mtune=cortex-a7 -I. -DTEXT_START=0x81000000
LDFLAGS= -Wl,-T$(PACKAGE).lds $(CFLAGS) -Wl,--build-id=none

SRCS:= start.S
CSRCS:= pb.c mini-printf.c
OBJS:=$(SRCS:.S=.o)
COBJS:=$(CSRCS:.c=.o)

all: $(PACKAGE).bin

$(PACKAGE).bin: $(PACKAGE)
	$(OBJCOPY) -O binary -R .comment $< $@
	@mkimage -n imximage.cfg -T imximage -a 0x81000000 -e 0x81000000 -d $(PACKAGE).bin $(PACKAGE).imx

$(PACKAGE): $(OBJS) $(COBJS)
	$(CC) $(LDFLAGS) $(OBJS)  $(COBJS) -o $@

%.o: %.S
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@


.PHONY: clean distclean
clean:
	@-rm -rf *.o $(PACKAGE) $(PACKAGE).bin *.map out

prog:
	@imx_usb ./pb.imx
