TESTS   = test_boot
TESTS  += test_usb_ch9
TESTS  += test_libc

INTEGRATION_TESTS  = test_reset
INTEGRATION_TESTS += test_part
INTEGRATION_TESTS += test_device_setup
INTEGRATION_TESTS += test_setup_lock
INTEGRATION_TESTS += test_corrupt_gpt
INTEGRATION_TESTS += test_corrupt_gpt2
INTEGRATION_TESTS += test_corrupt_gpt3
INTEGRATION_TESTS += test_corrupt_gpt4
INTEGRATION_TESTS += test_corrupt_gpt5
INTEGRATION_TESTS += test_part_flash
INTEGRATION_TESTS += test_boot_pbi
INTEGRATION_TESTS += test_boot_pbi2
INTEGRATION_TESTS += test_boot_pbi3
INTEGRATION_TESTS += test_boot_pbi4
INTEGRATION_TESTS += test_boot_pbi5
INTEGRATION_TESTS += test_boot_pbi6
INTEGRATION_TESTS += test_boot_pbi7
INTEGRATION_TESTS += test_boot_pbi8
INTEGRATION_TESTS += test_flash_bl
INTEGRATION_TESTS += test_invalid_key_index
INTEGRATION_TESTS += test_invalid_key_index2
INTEGRATION_TESTS += test_gpt_boot_activate
INTEGRATION_TESTS += test_gpt_boot_activate_step2
INTEGRATION_TESTS += test_gpt_boot_activate_step3
INTEGRATION_TESTS += test_gpt_boot_activate_step4
INTEGRATION_TESTS += test_gpt_boot_activate_step5
INTEGRATION_TESTS += test_rollback
INTEGRATION_TESTS += test_cli

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
TEST_C_SRCS += plat/test/pl061.c
TEST_C_SRCS += plat/test/gcov.c
TEST_C_SRCS += usb.c
TEST_C_SRCS += plat/test/semihosting.c
TEST_C_SRCS += plat/test/uart.c
TEST_C_SRCS += lib/printf.c
TEST_C_SRCS += lib/putchar.c
TEST_C_SRCS += lib/memcmp.c
TEST_C_SRCS += lib/strlen.c
TEST_C_SRCS += lib/memcpy.c
TEST_C_SRCS += lib/strcmp.c
TEST_C_SRCS += lib/snprintf.c

PB_PLAT_NAME = test
PB_BOARD_NAME = test

TEST_ASM_SRCS += plat/test/semihosting_call.S

TEST_OBJS      = $(TEST_C_SRCS:.c=.o) 
TEST_OBJS      += $(TEST_ASM_SRCS:.S=.o) 

CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += 

BOARD = test

test: $(ARCH_OBJS) $(TEST_OBJS) 
	@dd if=/dev/zero of=/tmp/disk bs=1M count=32 > /dev/null 2>&1
	@dd if=/dev/zero of=/tmp/disk_aux bs=1M count=16 > /dev/null 2>&1
	@make -C tools/punchboot CROSS_COMPILE="" TRANSPORT=socket CODE_COV=1
	@make -C tools/pbimage CROSS_COMPILE="" CODE_COV=1
	@rm -f test_log.txt
	@sync
	@$(foreach TEST,$(TESTS), \
		$(CC) $(CFLAGS) -c tests/$(TEST).c && \
		$(LD) $(LDFLAGS) $(ARCH_OBJS) $(TEST_OBJS) \
			 $(LIBS) $(TEST).o -o $(TEST) && \
		echo "TEST $(TEST)"  >> test_log.txt && \
		sync && \
		$(QEMU) $(QEMU_FLAGS) -kernel $(TEST) >> test_log.txt 2>&1; )

	@echo
	@echo
	@echo "Running test:"
	@$(foreach TEST,$(INTEGRATION_TESTS), \
		QEMU="$(QEMU)" QEMU_FLAGS="$(QEMU_FLAGS)" TEST_NAME="$(TEST)" \
			tests/$(TEST).sh || exit; ) 
	@echo
	@echo "*** ALL $(words ${TESTS} ${INTEGRATION_TESTS}) TESTS PASSED ***"

