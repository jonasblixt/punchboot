TESTS  = test_boot

INTEGRATION_TESTS  = test_reset
INTEGRATION_TESTS += test_part
INTEGRATION_TESTS += test_config
INTEGRATION_TESTS += test_corrupt_gpt
INTEGRATION_TESTS += test_corrupt_gpt2

QEMU = qemu-system-arm
QEMU_AUDIO_DRV = "none"
QEMU_FLAGS  = -machine virt -cpu cortex-a15 -m 1024 
QEMU_FLAGS += -nographic -semihosting
QEMU_FLAGS += -device virtio-serial-device  -chardev socket,path=/tmp/pb_sock,server,nowait,id=foo 
QEMU_FLAGS += -device virtserialport,chardev=foo  
QEMU_FLAGS += -device virtio-blk-device,drive=disk 
QEMU_FLAGS += -drive id=disk,file=/tmp/disk,if=none,format=raw

TEST_C_SRCS += tests/common.c

TEST_OBJS      = $(TEST_C_SRCS:.c=.o) 

CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += 

BOARD = test

test: $(ARCH_OBJS) $(PLAT_OBJS) $(BOARD_OBJS) $(TEST_OBJS) 
	@dd if=/dev/zero of=/tmp/disk bs=1M count=32
	@make -C tools/punchboot CROSS_COMPILE="" TRANSPORT=socket
	@make -C tools/pbimage CROSS_COMPILE=""
	@$(foreach TEST,$(TESTS), \
		$(CC) $(CFLAGS) -c tests/$(TEST).c && \
		$(LD) $(LDFLAGS) $(OBJS) $(TEST_OBJS) \
			 $(LIBS) $(TEST).o -o $(TEST) && \
		echo "TEST $(TEST)" && \
		$(QEMU) $(QEMU_FLAGS) -kernel $(TEST);)

	@$(foreach TEST,$(INTEGRATION_TESTS), \
		QEMU="$(QEMU)" QEMU_FLAGS="$(QEMU_FLAGS)" TEST_NAME="$(TEST)" tests/$(TEST).sh;)

	@echo "*** ALL $(words ${TESTS} ${INTEGRATION_TESTS}) TESTS PASSED ***"

