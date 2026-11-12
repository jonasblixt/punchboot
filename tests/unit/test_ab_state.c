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

/* The board's object for the policy, the way a board has it in its data
 * (board.boot_policy, F-265): BOOT_POLICY_AB() stays a constant initializer */
static const struct boot_policy policy = BOOT_POLICY_AB(&cfg_speculative);
static const struct boot_policy policy_normal = BOOT_POLICY_AB(&cfg_normal);
/* What a board can't write with the macro: an object without a configuration */
static const struct boot_policy no_config = { .ops = &boot_policy_ab_ops };

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

/*
 * One boot that goes well: the policy's first candidate, which loads and
 * verifies. Returns its device, or the policy's error when it has none.
 */
static bio_dev_t boot_once(void)
{
    uuid_t uu;
    int rc = boot_policy_ab_ops.next(uu);

    if (rc != PB_OK)
        return rc;

    boot_policy_ab_ops.report(uu, PB_OK);
    return bio_get_part_by_uu(uu);
}

/* The candidate the policy hands out, which then fails to load or verify */
static bio_dev_t candidate_fails(void)
{
    uuid_t uu;
    int rc = boot_policy_ab_ops.next(uu);

    if (rc != PB_OK)
        return rc;

    boot_policy_ab_ops.report(uu, -PB_ERR_BAD_PAYLOAD);
    return bio_get_part_by_uu(uu);
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

    /* Without a configuration there is no state either */
    assert_int_equal(boot_ab_state_init(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_policy_ab_ops.init(&no_config), -PB_ERR_PARAM);

    assert_int_equal(boot_once(), -PB_ERR_STATE);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
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
    /* Nothing enabled: no candidate */
    assert_int_equal(boot_once(), -PB_ERR_NOT_FOUND);
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

/* NOR flash: the state partitions are erased before they are written */
static struct ramdisk *erase_disk;
static lba_t erased[4];
static unsigned int erase_calls;
static int erase_rc;

static int fake_nor_erase(bio_dev_t dev, lba_t first_lba, size_t count)
{
    (void)dev;
    assert_int_equal(count, 1);
    if (erase_calls < 4)
        erased[erase_calls] = first_lba;
    erase_calls++;
    if (erase_rc != PB_OK)
        return erase_rc;
    memset(ramdisk_block(erase_disk, first_lba), 0xff, RAMDISK_BLOCK_SIZE);
    return PB_OK;
}

static void attach_nor(struct fixture *f)
{
    bio_reset();
    bio_dev_t disk = ramdisk_attach(f->disk, UU_DISK);
    assert_true(disk >= 0);
    assert_int_equal(bio_set_ios_erase(disk, fake_nor_erase), PB_OK);
    assert_int_equal(bio_clear_set_flags(disk, 0, BIO_FLAG_ERASE_BEFORE_WRITE), PB_OK);
    assert_true(bio_allocate_parent(disk, 10, 19, RAMDISK_BLOCK_SIZE, UU_SYS_A, "System A") >= 0);
    assert_true(bio_allocate_parent(disk, 20, 29, RAMDISK_BLOCK_SIZE, UU_SYS_B, "System B") >= 0);
    assert_true(bio_allocate_parent(
                    disk, PRIMARY_LBA, PRIMARY_LBA, RAMDISK_BLOCK_SIZE, UU_PRIMARY, "Primary") >=
                0);
    assert_true(bio_allocate_parent(
                    disk, BACKUP_LBA, BACKUP_LBA, RAMDISK_BLOCK_SIZE, UU_BACKUP, "Backup") >= 0);
    erase_disk = f->disk;
    erase_calls = 0;
    erase_rc = PB_OK;
}

static void test_commit_erases_first(void **state)
{
    struct fixture *f = *state;

    attach_nor(f);
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_int_equal(erase_calls, 2);
    assert_int_equal(erased[0], PRIMARY_LBA);
    assert_int_equal(erased[1], BACKUP_LBA);
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_true(state_valid(f, BACKUP_LBA));

    /* An erase error stops the commit before the write */
    attach_nor(f);
    memset(state_at(f, PRIMARY_LBA), 0, RAMDISK_BLOCK_SIZE);
    erase_rc = -PB_ERR_IO;
    assert_int_equal(boot_ab_state_init(&cfg_normal), -PB_ERR_IO);
    assert_int_equal(erase_calls, 1);
    assert_false(state_valid(f, PRIMARY_LBA));
}

static void test_verified_system_boots(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    /* Nothing to commit */
    assert_int_equal(f->disk->writes, 0);
}

static void test_b_boot_attempts(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, 0);
    reboot(f, &cfg_normal);

    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(state_at(f, BACKUP_LBA)->remaining_boot_attempts, 0);

    /* Out of attempts: roll back to the verified A */
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_B_ROLLBACK);
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_true(state_valid(f, BACKUP_LBA));
}

static void test_no_rollback_to_unverified_system(void **state)
{
    struct fixture *f = *state;

    /* No viable candidate, and nothing is written */
    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), -PB_ERR_NOT_FOUND);

    write_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), -PB_ERR_NOT_FOUND);
    assert_int_equal(f->disk->writes, 0);
}

static void test_speculative_rollback_from_a(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_speculative);

    /* B gets one attempt, then it's A's turn again */
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_A_ROLLBACK);
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(boot_once(), f->sys_a);
}

static void test_speculative_rollback_from_b(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    reboot(f, &cfg_speculative);

    assert_int_equal(boot_once(), f->sys_a);
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
    assert_int_equal(boot_once(), -PB_ERR_IO);
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

/* The policy the way src/board.c uses it: only through the board's object
 * (F-265) */
static void test_policy_object(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    assert_ptr_equal(policy.ops, &boot_policy_ab_ops);
    assert_ptr_equal(policy.config, &cfg_speculative);

    /* The module has the other configuration, so that what get() does below
     * shows that the object's reached it */
    write_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);

    /* An error of the module's is init's answer */
    attach(f, false, true);
    assert_int_equal(policy.ops->init(&policy), -PB_ERR_NOT_FOUND);

    attach(f, true, true);
    assert_int_equal(policy.ops->init(&policy), PB_OK);

    /* B isn't verified and has no attempts left: only the speculative mode
     * boots A */
    assert_int_equal(boot_once(), f->sys_a);
    policy.ops->status(uu);
    assert_memory_equal(uu, UU_SYS_A, 16);

    /* A host activates B */
    assert_int_equal(policy.ops->activate((unsigned char *)UU_SYS_B), PB_OK);
    assert_int_equal(boot_once(), f->sys_b);
    policy.ops->status(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);
    assert_int_equal(state_at(f, PRIMARY_LBA)->verified, PB_STATE_B_VERIFIED);

    /* A partition that isn't a system: nothing is active */
    assert_int_equal(policy.ops->activate((unsigned char *)UU_OTHER), -PB_ERR_PART_NOT_BOOTABLE);
    policy.ops->status(uu);
    assert_true(uuid_is_null(uu));
}

/* init marks the policy's candidates, the two slots, and nothing else: only
 * they can be booted, also by 'boot part' (boot.md 3.1, F-215) */
static void test_init_marks_the_candidates(void **state)
{
    struct fixture *f = *state;

    assert_false(bio_get_flags(f->sys_a) & BIO_FLAG_BOOTABLE);
    assert_false(bio_get_flags(f->sys_b) & BIO_FLAG_BOOTABLE);

    assert_int_equal(policy_normal.ops->init(&policy_normal), PB_OK);

    assert_true(bio_get_flags(f->sys_a) & BIO_FLAG_BOOTABLE);
    assert_true(bio_get_flags(f->sys_b) & BIO_FLAG_BOOTABLE);
    assert_false(bio_get_flags(bio_get_part_by_uu(UU_PRIMARY)) & BIO_FLAG_BOOTABLE);
    assert_false(bio_get_flags(bio_get_part_by_uu(UU_BACKUP)) & BIO_FLAG_BOOTABLE);
    assert_false(bio_get_flags(bio_get_part_by_uu(UU_DISK)) & BIO_FLAG_BOOTABLE);

    /* It can run again (after a table is installed, boot.md 3.1) */
    assert_int_equal(policy_normal.ops->init(&policy_normal), PB_OK);
    assert_true(bio_get_flags(f->sys_a) & BIO_FLAG_BOOTABLE);
}

/* The first candidate fails in this boot: the other slot, when it is
 * verified, for this boot only and with nothing written (boot.md 3.5, rule
 * 3, F-174) */
static void test_fallback_to_the_verified_slot(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);

    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);

    assert_int_equal(boot_policy_ab_ops.next(uu), PB_OK);
    assert_memory_equal(uu, UU_SYS_B, 16);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_FALLBACK);
    boot_policy_ab_ops.report(uu, PB_OK);
    /* The handover reads the mode after the report */
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_FALLBACK);

    /* Nothing was written: A is still the enabled slot */
    assert_int_equal(f->disk->writes, 0);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    boot_policy_ab_ops.status(uu);
    assert_memory_equal(uu, UU_SYS_A, 16);

    /* The next boot tries A first again, also without a reset in between */
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);

    /* The same from B */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_FALLBACK);
    assert_int_equal(f->disk->writes, 0);
}

/* Both fail: no third candidate, and the next boot starts over */
static void test_fallback_fails_too(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);

    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(f->disk->writes, 0);

    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
}

/* No fallback to a slot the OS never confirmed */
static void test_no_fallback_to_unverified_slot(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    /* What 'boot activate A' leaves: only A verified */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);

    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(f->disk->writes, 0);

    /* The next boot: A again */
    assert_int_equal(boot_once(), f->sys_a);
}

/* A trial that fails to load spent its attempt, and the verified slot boots
 * for this boot. The trial is tried again on the next boots until its
 * attempts run out, then it rolls back */
static void test_trial_that_fails_falls_back(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, 0);
    reboot(f, &cfg_normal);

    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    unsigned int writes = f->disk->writes;

    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_FALLBACK);
    /* The fallback wrote nothing: B is still enabled, on trial */
    assert_int_equal(f->disk->writes, writes);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, 0);

    /* The next boot: B's last attempt */
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 0);
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);

    /* Out of attempts: the rollback, which is written, and mode normal */
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_B_ROLLBACK);
}

/* A trial that fails to load, and no slot the OS confirmed: the attempt is
 * spent and written, and there is nothing else to try */
static void test_trial_that_fails_without_a_verified_slot(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_B_ENABLED, 0, 2, 0);
    reboot(f, &cfg_normal);

    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    assert_int_equal(state_at(f, BACKUP_LBA)->remaining_boot_attempts, 1);
    unsigned int writes = f->disk->writes;

    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(f->disk->writes, writes);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, 0);

    /* The next boot: its last attempt, the same */
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 0);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);

    /* Out of attempts and nothing to roll back to: no candidate at all, and
     * nothing more is written */
    writes = f->disk->writes;
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(f->disk->writes, writes);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
}

/* With the board's rollback_mode set to speculative, as released. F-257
 * replaces that mode (boot.md 3.6): these pin what the code does today */
static void test_fallback_with_speculative_rollback_mode(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    /* Rule 3 doesn't depend on the mode: the verified slot, nothing written */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_speculative);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_FALLBACK);
    assert_int_equal(f->disk->writes, 0);

    /* The speculative rollback hands out the slot that was never confirmed
     * as the boot's first candidate, written, with one attempt. Its mode
     * reads normal: speculative is F-257's */
    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_speculative);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_A_ROLLBACK);

    /* It fails: A isn't verified, so it is no fallback in the same boot.
     * The mode alternates only over boots */
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);

    /* The next boot spends B's one attempt, the one after goes back to A */
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 0);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_B_ROLLBACK);
}

/* The slot a rollback handed out fails: the other one is the trial that
 * ran out, no candidate */
static void test_rollback_target_fails(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);

    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
}

/* A state write that fails ends the candidates (F-179 changes that): the
 * next boot starts over */
static void test_next_starts_over_after_an_error(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, PB_STATE_B_VERIFIED, 3, 0);
    reboot(f, &cfg_normal);

    f->disk->fail_writes_after = 0;
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_IO);
    f->disk->fail_writes_after = -1;

    /* Not B as a fallback: A, as the first candidate of a new boot */
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
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
        AB_TEST(test_commit_erases_first),
        AB_TEST(test_verified_system_boots),
        AB_TEST(test_b_boot_attempts),
        AB_TEST(test_no_rollback_to_unverified_system),
        AB_TEST(test_speculative_rollback_from_a),
        AB_TEST(test_speculative_rollback_from_b),
        AB_TEST(test_get_commit_error),
        AB_TEST(test_set_boot_partition),
        AB_TEST(test_part_names),
        AB_TEST(test_board_regs),
        AB_TEST(test_policy_object),
        AB_TEST(test_init_marks_the_candidates),
        AB_TEST(test_fallback_to_the_verified_slot),
        AB_TEST(test_fallback_fails_too),
        AB_TEST(test_no_fallback_to_unverified_slot),
        AB_TEST(test_trial_that_fails_falls_back),
        AB_TEST(test_trial_that_fails_without_a_verified_slot),
        AB_TEST(test_fallback_with_speculative_rollback_mode),
        AB_TEST(test_rollback_target_fails),
        AB_TEST(test_next_starts_over_after_an_error),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
