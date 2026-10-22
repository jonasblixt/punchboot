/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A/B boot state (src/boot/ab_state.c) on a ramdisk. The integration tests
 * (test_ab_state.py) cover switching and the common rollback; these cover the
 * other rollback cases, the speculative mode and I/O errors.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/ab_state.h>
#include <boot/pb_state_blob.h>
#include <pb/bio.h>
#include <pb/crc.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/ramdisk.h"

#define DISK_BLOCKS 64
#define PRIMARY_LBA 30
#define BACKUP_LBA  31

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
#define UU_SYS_A \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define UU_SYS_B \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define UU_PRIMARY \
    (const unsigned char *)"\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"
#define UU_BACKUP \
    (const unsigned char *)"\x65\x6a\xb3\xfc\x58\x56\x4a\x5e\xa2\xae\x5a\x01\x83\x13\xb3\xee"
#define UU_OTHER \
    (const unsigned char *)"\xff\x4d\xdc\x6c\xad\x7a\x47\xe8\x87\x73\x67\x29\x39\x2d\xd1\xb5"

static const struct boot_ab_state_config cfg_normal = {
    .primary_state_part_uu = UU_PRIMARY,
    .backup_state_part_uu = UU_BACKUP,
    .sys_a_uu = UU_SYS_A,
    .sys_b_uu = UU_SYS_B,
    .rollback_mode = AB_ROLLBACK_MODE_NORMAL,
};

static const struct boot_ab_state_config cfg_speculative = {
    .primary_state_part_uu = UU_PRIMARY,
    .backup_state_part_uu = UU_BACKUP,
    .sys_a_uu = UU_SYS_A,
    .sys_b_uu = UU_SYS_B,
    .rollback_mode = AB_ROLLBACK_MODE_SPECULATIVE,
};

struct fixture {
    struct ramdisk *disk;
    bio_dev_t sys_a;
    bio_dev_t sys_b;
};

/* Partitions: System A, System B, primary and backup state. Called again to
 * simulate a reboot. 'with_*' leave out partitions. */
static void attach(struct fixture *f, bool with_sys_b, bool with_state)
{
    bio_reset();
    bio_dev_t disk = ramdisk_attach(f->disk, UU_DISK);
    assert_true(disk >= 0);
    f->sys_a = bio_allocate_parent(disk, 10, 19, RAMDISK_BLOCK_SIZE, UU_SYS_A, "System A");
    assert_true(f->sys_a >= 0);
    if (with_sys_b) {
        f->sys_b = bio_allocate_parent(disk, 20, 29, RAMDISK_BLOCK_SIZE, UU_SYS_B, "System B");
        assert_true(f->sys_b >= 0);
    }
    if (with_state) {
        assert_true(
            bio_allocate_parent(
                disk, PRIMARY_LBA, PRIMARY_LBA, RAMDISK_BLOCK_SIZE, UU_PRIMARY, "Primary") >= 0);
        assert_true(bio_allocate_parent(
                        disk, BACKUP_LBA, BACKUP_LBA, RAMDISK_BLOCK_SIZE, UU_BACKUP, "Backup") >=
                    0);
    }
}

static struct pb_boot_state *state_at(struct fixture *f, lba_t lba)
{
    return (struct pb_boot_state *)ramdisk_block(f->disk, lba);
}

static bool state_valid(struct fixture *f, lba_t lba)
{
    struct pb_boot_state s;

    memcpy(&s, state_at(f, lba), sizeof(s));
    uint32_t crc = s.crc;
    s.crc = 0;
    return (s.magic == PB_STATE_MAGIC) && (crc == crc32(0, (const uint8_t *)&s, sizeof(s)));
}

/* Write a valid state to both copies, like pbstate does */
static void write_state(struct fixture *f,
                        uint32_t enable,
                        uint32_t verified,
                        uint32_t attempts,
                        uint32_t error)
{
    struct pb_boot_state s = { 0 };

    s.magic = PB_STATE_MAGIC;
    s.enable = enable;
    s.verified = verified;
    s.remaining_boot_attempts = attempts;
    s.error = error;
    s.crc = crc32(0, (const uint8_t *)&s, sizeof(s));
    memcpy(state_at(f, PRIMARY_LBA), &s, sizeof(s));
    memcpy(state_at(f, BACKUP_LBA), &s, sizeof(s));
}

static void reboot(struct fixture *f, const struct boot_ab_state_config *cfg)
{
    attach(f, true, true);
    assert_int_equal(boot_ab_state_init(cfg), PB_OK);
}

static int setup(void **state)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
    f->disk = ramdisk_new(DISK_BLOCKS);
    assert_non_null(f->disk);
    attach(f, true, true);
    *state = f;
    return 0;
}

static int teardown(void **state)
{
    struct fixture *f = *state;
    ramdisk_free(f->disk);
    free(f);
    return 0;
}

/* Runs first: the module has no config yet */
static void test_not_initialized(void **state)
{
    uuid_t uu;
    uint32_t value;
    (void)state;

    assert_int_equal(boot_ab_state_get(), -PB_ERR_STATE);
    assert_int_equal(boot_ab_state_set_boot_partition((unsigned char *)UU_SYS_A), -PB_ERR_STATE);
    memset(uu, 0xff, sizeof(uu));
    boot_ab_state_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));
    assert_string_equal(boot_ab_part_uu_to_name((unsigned char *)UU_SYS_A), "?");
    assert_int_equal(boot_ab_state_write_board_reg(0, 1), -PB_ERR_STATE);
    assert_int_equal(boot_ab_state_read_board_reg(0, &value), PB_OK);
}

static void test_init_requires_system_partitions(void **state)
{
    struct fixture *f = *state;

    attach(f, false, true);
    assert_int_equal(boot_ab_state_init(&cfg_normal), -PB_ERR_NOT_FOUND);

    bio_reset();
    assert_true(ramdisk_attach(f->disk, UU_DISK) >= 0);
    assert_int_equal(boot_ab_state_init(&cfg_normal), -PB_ERR_NOT_FOUND);
}

static void test_init_without_state_partitions_fails(void **state)
{
    struct fixture *f = *state;

    attach(f, true, false);
    assert_int_not_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    /* System A and B are bootable anyway */
    assert_true(bio_get_flags(f->sys_a) & BIO_FLAG_BOOTABLE);
    assert_true(bio_get_flags(f->sys_b) & BIO_FLAG_BOOTABLE);
}

static void test_init_blank_installs_defaults(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_true(state_valid(f, BACKUP_LBA));
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, 0);
    assert_int_equal(boot_ab_state_get(), -PB_ERR_NO_ACTIVE_BOOT_PARTITION);
}

static void test_init_bad_crc_is_repaired(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    state_at(f, PRIMARY_LBA)->crc ^= 1;

    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
}

static void test_commit_write_errors(void **state)
{
    struct fixture *f = *state;

    /* Blank disk: init writes the defaults, first the primary */
    f->disk->fail_writes_after = 0;
    assert_int_equal(boot_ab_state_init(&cfg_normal), -PB_ERR_IO);

    f->disk->fail_writes_after = 1;
    assert_int_equal(boot_ab_state_init(&cfg_normal), -PB_ERR_IO);
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_false(state_valid(f, BACKUP_LBA));

    f->disk->fail_writes_after = -1;
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_true(state_valid(f, BACKUP_LBA));
}

static void test_verified_system_boots(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_ab_state_get(), f->sys_a);

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_ab_state_get(), f->sys_b);
    /* Nothing to commit */
    assert_int_equal(f->disk->writes, 0);
}

static void test_b_boot_attempts(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, 0);
    reboot(f, &cfg_normal);

    assert_int_equal(boot_ab_state_get(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    assert_int_equal(boot_ab_state_get(), f->sys_b);
    assert_int_equal(state_at(f, BACKUP_LBA)->remaining_boot_attempts, 0);

    /* Out of attempts: roll back to the verified A */
    assert_int_equal(boot_ab_state_get(), f->sys_a);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_B_ROLLBACK);
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_true(state_valid(f, BACKUP_LBA));
}

static void test_no_rollback_to_unverified_system(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_ab_state_get(), -PB_ERR);

    write_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_ab_state_get(), -PB_ERR);
}

static void test_speculative_rollback_from_a(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_speculative);

    /* B gets one attempt, then it's A's turn again */
    assert_int_equal(boot_ab_state_get(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_A_ROLLBACK);
    assert_int_equal(boot_ab_state_get(), f->sys_b);
    assert_int_equal(boot_ab_state_get(), f->sys_a);
}

static void test_speculative_rollback_from_b(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    reboot(f, &cfg_speculative);

    assert_int_equal(boot_ab_state_get(), f->sys_a);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_B_ROLLBACK);
}

static void test_get_commit_error(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_A_ENABLED, 0, 3, 0);
    reboot(f, &cfg_normal);

    f->disk->fail_writes_after = 0;
    assert_int_equal(boot_ab_state_get(), -PB_ERR_IO);
}

static void test_set_boot_partition(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    reboot(f, &cfg_normal);

    assert_int_equal(boot_ab_state_set_boot_partition((unsigned char *)UU_SYS_B), PB_OK);
    boot_ab_state_get_boot_partition(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);
    assert_int_equal(state_at(f, PRIMARY_LBA)->verified, PB_STATE_B_VERIFIED);

    assert_int_equal(boot_ab_state_set_boot_partition((unsigned char *)UU_SYS_A), PB_OK);
    boot_ab_state_get_boot_partition(uu);
    assert_memory_equal(uu, UU_SYS_A, 16);

    /* An unknown partition disables boot, but isn't committed */
    unsigned int writes = f->disk->writes;
    assert_int_equal(boot_ab_state_set_boot_partition((unsigned char *)UU_OTHER),
                     -PB_ERR_PART_NOT_BOOTABLE);
    assert_int_equal(f->disk->writes, writes);
    boot_ab_state_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));
}

static void test_part_names(void **state)
{
    struct fixture *f = *state;

    reboot(f, &cfg_normal);
    assert_string_equal(boot_ab_part_uu_to_name((unsigned char *)UU_SYS_A), "A");
    assert_string_equal(boot_ab_part_uu_to_name((unsigned char *)UU_SYS_B), "B");
    assert_string_equal(boot_ab_part_uu_to_name((unsigned char *)UU_OTHER), "?");
}

static void test_board_regs(void **state)
{
    struct fixture *f = *state;
    uint32_t value = 0;

    reboot(f, &cfg_normal);

    assert_int_equal(boot_ab_state_write_board_reg(3, 0xcafe), PB_OK);
    assert_int_equal(boot_ab_state_read_board_reg(3, &value), PB_OK);
    assert_int_equal(value, 0xcafe);
    /* Registers are stored from the end */
    assert_int_equal(state_at(f, PRIMARY_LBA)->board_regs[0], 0xcafe);

    assert_int_equal(boot_ab_state_write_board_reg(PB_STATE_NO_OF_BOARD_REGS, 1), -PB_ERR_PARAM);
    assert_int_equal(boot_ab_state_read_board_reg(PB_STATE_NO_OF_BOARD_REGS, &value),
                     -PB_ERR_PARAM);
}

#define AB_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        AB_TEST(test_not_initialized),
        AB_TEST(test_init_requires_system_partitions),
        AB_TEST(test_init_without_state_partitions_fails),
        AB_TEST(test_init_blank_installs_defaults),
        AB_TEST(test_init_bad_crc_is_repaired),
        AB_TEST(test_commit_write_errors),
        AB_TEST(test_verified_system_boots),
        AB_TEST(test_b_boot_attempts),
        AB_TEST(test_no_rollback_to_unverified_system),
        AB_TEST(test_speculative_rollback_from_a),
        AB_TEST(test_speculative_rollback_from_b),
        AB_TEST(test_get_commit_error),
        AB_TEST(test_set_boot_partition),
        AB_TEST(test_part_names),
        AB_TEST(test_board_regs),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
