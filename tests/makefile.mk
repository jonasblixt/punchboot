# Unit tests

TESTS   = test_boot
TESTS  += test_usb_ch9
TESTS  += test_libc
TESTS  += test_asn1
TESTS  += test_fletcher

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
INTEGRATION_TESTS += test_boot_bpak10
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
INTEGRATION_TESTS += test_board
INTEGRATION_TESTS += test_board_status
INTEGRATION_TESTS += test_all_sig_formats
INTEGRATION_TESTS += test_authentication
INTEGRATION_TESTS += test_revoke_key
INTEGRATION_TESTS += test_part_dump
INTEGRATION_TESTS += test_part_dump2
INTEGRATION_TESTS += test_gpt_resize
INTEGRATION_TESTS += test_gpt_resize2
INTEGRATION_TESTS += test_gpt_resize3


TEST_ASM_SRCS += src/arch/armv7a/entry_armv7a.S
TEST_ASM_SRCS += src/arch/armv7a/arm32_aeabi_divmod_a32.S
TEST_ASM_SRCS += src/arch/armv7a/uldivmod.S
TEST_ASM_SRCS += src/arch/armv7a/boot.S
TEST_ASM_SRCS += src/arch/armv7a/timer.S
TEST_ASM_SRCS += src/arch/armv7a/cp15.S
TEST_ASM_SRCS += src/arch/armv7a/misc_helpers.S

TEST_C_SRCS += src/arch/armv7a/arm32_aeabi_divmod.c
TEST_C_SRCS += src/arch/armv7a/arch.c


TEST_C_SRCS += tests/common.c
TEST_C_SRCS += src/plat/qemu/gcov.c
TEST_C_SRCS += src/plat/qemu/reset.c
TEST_C_SRCS += src/usb.c
TEST_C_SRCS += src/asn1.c
TEST_C_SRCS += src/plat/qemu/semihosting.c
TEST_C_SRCS += src/plat/qemu/uart.c
TEST_C_SRCS += src/libc/printf.c
TEST_C_SRCS += src/libc/memcmp.c
TEST_C_SRCS += src/libc/strlen.c
TEST_C_SRCS += src/libc/memcpy.c
TEST_C_SRCS += src/libc/memset.c
TEST_C_SRCS += src/libc/strtoul.c
TEST_C_SRCS += src/libc/strcmp.c
TEST_C_SRCS += src/libc/snprintf.c
TEST_C_SRCS += src/libc/putchar.c
TEST_C_SRCS += src/libc/assert.c
TEST_C_SRCS += src/fletcher.c

# UUID lib
TEST_C_SRCS  += src/uuid/pack.c
TEST_C_SRCS  += src/uuid/unpack.c
TEST_C_SRCS  += src/uuid/compare.c
TEST_C_SRCS  += src/uuid/copy.c
TEST_C_SRCS  += src/uuid/unparse.c
TEST_C_SRCS  += src/uuid/parse.c
TEST_C_SRCS  += src/uuid/clear.c
TEST_C_SRCS  += src/uuid/conv.c

TEST_ASM_SRCS += src/plat/qemu/semihosting_call.S

TEST_OBJS    = $(patsubst %.S, $(BUILD_DIR)/%.o, $(TEST_ASM_SRCS))
TEST_OBJS   += $(patsubst %.c, $(BUILD_DIR)/%.o, $(TEST_C_SRCS))

cflags-y += -fprofile-arcs -ftest-coverage

check: all $(ARCH_OBJS) $(TEST_OBJS)
	@mkdir -p $(BUILD_DIR)/tests
	@dd if=/dev/zero of=$(CONFIG_QEMU_VIRTIO_DISK) bs=1M \
		count=$(CONFIG_QEMU_VIRTIO_DISK_SIZE_MB) > /dev/null 2>&1
	@sync
	$(Q)$(foreach TEST,$(TESTS), \
		$(CC) $(cflags-y) -c tests/$(TEST).c -o $(BUILD_DIR)/tests/$(TEST).o && \
		$(LD) $(ldflags-y) $(ARCH_OBJS) $(TEST_OBJS) \
			  $(BUILD_DIR)/tests/$(TEST).o $(LIBS) -o $(BUILD_DIR)/tests/$(TEST) || exit; )

	$(Q)$(foreach TEST,$(TESTS), \
		echo "--- Module TEST ---  $(TEST)"  && \
		$(QEMU) $(QEMU_FLAGS) -kernel $(BUILD_DIR)/tests/$(TEST) || exit; )

	@echo
	@echo
	@echo "Running test:"
	$(Q)$(foreach TEST,$(INTEGRATION_TESTS), \
		QEMU="$(QEMU)" QEMU_FLAGS="$(QEMU_FLAGS)" TEST_NAME="$(TEST)" \
			tests/$(TEST).sh || exit; )
	@echo
	@echo "*** ALL $(words ${TESTS} ${INTEGRATION_TESTS}) TESTS PASSED ***"

debug_test: $(ARCH_OBJS) $(TEST_OBJS)
	@echo "Debugging test $(TEST)"
	@QEMU="$(QEMU)" QEMU_FLAGS="$(QEMU_FLAGS) $(QEMU_AUX_FLAGS)" TEST_NAME="$(TEST)" \
			tests/$(TEST).sh

module_tests:
	$(Q)$(foreach TEST,$(TESTS), \
		$(CC) $(cflags-y) -c tests/$(TEST).c -o $(BUILD_DIR)/tests/$(TEST).o && \
		$(LD) $(ldflags-y) $(ARCH_OBJS) $(TEST_OBJS) \
			  $(BUILD_DIR)/tests/$(TEST).o $(LIBS) -o $(BUILD_DIR)/tests/$(TEST) || exit; )

	$(Q)$(foreach TEST,$(TESTS), \
		echo "--- Module TEST ---  $(TEST)"  && \
		$(QEMU) $(QEMU_FLAGS) -kernel $(BUILD_DIR)/tests/$(TEST) || exit; )
