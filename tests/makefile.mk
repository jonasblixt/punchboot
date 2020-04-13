ifdef CONFIG_BOARD_TEST
# Unit tests

TESTS   = test_boot
TESTS  += test_usb_ch9
TESTS  += test_libc

# Integration tests

INTEGRATION_TESTS  = test_reset
INTEGRATION_TESTS += test_part
INTEGRATION_TESTS += test_device_setup
INTEGRATION_TESTS += test_corrupt_gpt
INTEGRATION_TESTS += test_corrupt_gpt2
INTEGRATION_TESTS += test_corrupt_gpt3
INTEGRATION_TESTS += test_corrupt_gpt4
INTEGRATION_TESTS += test_corrupt_gpt5
INTEGRATION_TESTS += test_part_flash
INTEGRATION_TESTS += test_boot_bpak
INTEGRATION_TESTS += test_boot_bpak2
INTEGRATION_TESTS += test_boot_bpak3
INTEGRATION_TESTS += test_boot_bpak4
INTEGRATION_TESTS += test_boot_bpak5
INTEGRATION_TESTS += test_boot_bpak6
INTEGRATION_TESTS += test_boot_bpak7
INTEGRATION_TESTS += test_boot_bpak8
INTEGRATION_TESTS += test_boot_bpak9
INTEGRATION_TESTS += test_verify_bpak
INTEGRATION_TESTS += test_bpak_show
INTEGRATION_TESTS += test_invalid_key_index
INTEGRATION_TESTS += test_gpt_boot_activate
INTEGRATION_TESTS += test_gpt_boot_activate_step2
INTEGRATION_TESTS += test_gpt_boot_activate_step3
INTEGRATION_TESTS += test_gpt_boot_activate_step4
INTEGRATION_TESTS += test_gpt_boot_activate_step5
INTEGRATION_TESTS += test_rollback
INTEGRATION_TESTS += test_cli
INTEGRATION_TESTS += test_part_offset_write
INTEGRATION_TESTS += test_overlapping_region
INTEGRATION_TESTS += test_corrupt_backup_gpt
INTEGRATION_TESTS += test_corrupt_primary_config
INTEGRATION_TESTS += test_corrupt_backup_config
INTEGRATION_TESTS += test_switch
INTEGRATION_TESTS += test_authentication

TEST_C_SRCS += tests/common.c
TEST_C_SRCS += plat/qemu/gcov.c
TEST_C_SRCS += usb.c
TEST_C_SRCS += plat/qemu/semihosting.c
TEST_C_SRCS += plat/qemu/uart.c
TEST_C_SRCS += lib/printf.c
TEST_C_SRCS += lib/memcmp.c
TEST_C_SRCS += lib/strlen.c
TEST_C_SRCS += lib/memcpy.c
TEST_C_SRCS += lib/memset.c
TEST_C_SRCS += lib/strtoul.c
TEST_C_SRCS += lib/strcmp.c
TEST_C_SRCS += lib/snprintf.c
TEST_C_SRCS += lib/putchar.c

# UUID lib
TEST_C_SRCS  += uuid/pack.c
TEST_C_SRCS  += uuid/unpack.c
TEST_C_SRCS  += uuid/compare.c
TEST_C_SRCS  += uuid/copy.c
TEST_C_SRCS  += uuid/unparse.c
TEST_C_SRCS  += uuid/parse.c
TEST_C_SRCS  += uuid/clear.c
TEST_C_SRCS  += uuid/conv.c

TEST_ASM_SRCS += plat/qemu/semihosting_call.S

TEST_OBJS    = $(patsubst %.S, $(BUILD_DIR)/%.o, $(TEST_ASM_SRCS))
TEST_OBJS   += $(patsubst %.c, $(BUILD_DIR)/%.o, $(TEST_C_SRCS))

CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS +=

BOARD = test

check: all $(ARCH_OBJS) $(TEST_OBJS)
	@mkdir -p $(BUILD_DIR)/tests
	@dd if=/dev/zero of=/tmp/disk bs=1M count=32 > /dev/null 2>&1
	@sync
	@$(foreach TEST,$(TESTS), \
		$(CC) $(CFLAGS) -c tests/$(TEST).c -o $(BUILD_DIR)/tests/$(TEST).o && \
		$(LD) $(LDFLAGS) $(ARCH_OBJS) $(TEST_OBJS) \
			  $(BUILD_DIR)/tests/$(TEST).o $(LIBS) -o $(BUILD_DIR)/tests/$(TEST) || exit; )

	@$(foreach TEST,$(TESTS), \
		echo "--- Module TEST ---  $(TEST)"  && \
		$(QEMU) $(QEMU_FLAGS) -kernel $(BUILD_DIR)/tests/$(TEST) || exit; )

	@echo
	@echo
	@echo "Running test:"
	@$(foreach TEST,$(INTEGRATION_TESTS), \
		QEMU="$(QEMU)" QEMU_FLAGS="$(QEMU_FLAGS)" TEST_NAME="$(TEST)" \
			tests/$(TEST).sh || exit; )
	@echo
	@echo "*** ALL $(words ${TESTS} ${INTEGRATION_TESTS}) TESTS PASSED ***"

debug_test: $(ARCH_OBJS) $(TEST_OBJS)
	@echo "Debugging test $(TEST)"
	@QEMU="$(QEMU)" QEMU_FLAGS="$(QEMU_FLAGS) $(QEMU_AUX_FLAGS)" TEST_NAME="$(TEST)" \
			tests/$(TEST).sh

module_tests:
	$(foreach TEST,$(TESTS), \
		$(CC) $(CFLAGS) -c tests/$(TEST).c -o $(BUILD_DIR)/tests/$(TEST).o && \
		$(LD) $(LDFLAGS) $(ARCH_OBJS) $(TEST_OBJS) \
			  $(BUILD_DIR)/tests/$(TEST).o $(LIBS) -o $(BUILD_DIR)/tests/$(TEST) || exit; )

	@$(foreach TEST,$(TESTS), \
		echo "--- Module TEST ---  $(TEST)"  && \
		$(QEMU) $(QEMU_FLAGS) -kernel $(BUILD_DIR)/tests/$(TEST) || exit; )

endif
