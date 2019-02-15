TESTS  = test_boot

INTEGRATION_TESTS  = test_reset
INTEGRATION_TESTS += test_device_setup
INTEGRATION_TESTS += test_part
INTEGRATION_TESTS += test_corrupt_gpt
INTEGRATION_TESTS += test_corrupt_gpt2
INTEGRATION_TESTS += test_part_flash
INTEGRATION_TESTS += test_boot_pbi
INTEGRATION_TESTS += test_boot_pbi2
INTEGRATION_TESTS += test_boot_pbi3
INTEGRATION_TESTS += test_boot_pbi4
INTEGRATION_TESTS += test_boot_pbi5
INTEGRATION_TESTS += test_flash_bl
INTEGRATION_TESTS += test_invalid_key_index
INTEGRATION_TESTS += test_gpt_boot_activate
INTEGRATION_TESTS += test_gpt_boot_activate_step2
INTEGRATION_TESTS += test_gpt_boot_activate_step3
INTEGRATION_TESTS += test_gpt_boot_activate_step4
INTEGRATION_TESTS += test_gpt_boot_activate_step5

QEMU = qemu-system-arm
QEMU_AUDIO_DRV = "none"
QEMU_FLAGS  = -machine virt -cpu cortex-a15 -m 1024 
QEMU_FLAGS += -nographic -semihosting
# Virtio serial port
QEMU_FLAGS += -device virtio-serial-device  
QEMU_FLAGS += -chardev socket,path=/tmp/pb_sock,server,nowait,id=foo 
QEMU_FLAGS += -device virtserialport,chardev=foo  
# Virtio Main disk
QEMU_FLAGS += -device virtio-blk-device,drive=disk 
QEMU_FLAGS += -drive id=disk,file=/tmp/disk,if=none,format=raw
# Virtio Aux disk, for bootloader and fuses
QEMU_FLAGS += -device virtio-blk-device,drive=disk_aux
QEMU_FLAGS += -drive id=disk_aux,file=/tmp/disk_aux,if=none,format=raw

TEST_C_SRCS += tests/common.c

TEST_OBJS      = $(TEST_C_SRCS:.c=.o) 

CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += 

BOARD = test

test: $(ARCH_OBJS) $(PLAT_OBJS) $(BOARD_OBJS) $(TEST_OBJS) 
	@dd if=/dev/zero of=/tmp/disk bs=1M count=32
	@dd if=/dev/zero of=/tmp/disk_aux bs=1M count=16
	@make -C tools/punchboot CROSS_COMPILE="" TRANSPORT=socket CODE_COV=1
	@make -C tools/pbimage CROSS_COMPILE="" CODE_COV=1
	@$(foreach TEST,$(TESTS), \
		$(CC) $(CFLAGS) -c tests/$(TEST).c && \
		$(LD) $(LDFLAGS) $(OBJS) $(TEST_OBJS) \
			 $(LIBS) $(TEST).o -o $(TEST) && \
		echo "TEST $(TEST)" && \
		$(QEMU) $(QEMU_FLAGS) -kernel $(TEST);)

	@$(foreach TEST,$(INTEGRATION_TESTS), \
		QEMU="$(QEMU)" QEMU_FLAGS="$(QEMU_FLAGS)" TEST_NAME="$(TEST)" \
			tests/$(TEST).sh || exit; )

	@echo "*** ALL $(words ${TESTS} ${INTEGRATION_TESTS}) TESTS PASSED ***"

