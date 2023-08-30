
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
INTEGRATION_TESTS += test_gpt_variants
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
INTEGRATION_TESTS += test_board_regs

check: all
	@mkdir -p $(BUILD_DIR)/tests
	@dd if=/dev/zero of=$(CONFIG_QEMU_VIRTIO_DISK) bs=1M \
		count=$(CONFIG_QEMU_VIRTIO_DISK_SIZE_MB) > /dev/null 2>&1
	@sync
	@echo
	@echo
	@echo "Running test:"
	$(Q)$(foreach TEST,$(INTEGRATION_TESTS), \
		QEMU="$(QEMU)" QEMU_FLAGS="$(QEMU_FLAGS)" TEST_NAME="$(TEST)" \
			tests/$(TEST).sh || exit; )
	@echo
	@echo "*** ALL $(words ${TESTS} ${INTEGRATION_TESTS}) TESTS PASSED ***"
