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
 *
 * Built twice (CMakeLists.txt): test_ab_state without
 * CONFIG_BOOT_AB_SPECULATIVE, as pb-host is, and test_ab_state_speculative
 * with it. What A/B does with a viable candidate runs in both. Where it has
 * none, the first has the cases that end without a candidate and the second
 * speculative mode's, one test per rule of boot.md 3.6.
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
};

/* The slots the other way round: its "A" is the partition System B. What
 * tells the object's configuration from cfg_normal in test_policy_object */
static const struct boot_ab_state_config cfg_swapped = {
    .primary_state_part_uu = UU_PRIMARY,
    .backup_state_part_uu = UU_BACKUP,
    .sys_a_uu = UU_SYS_B,
    .sys_b_uu = UU_SYS_A,
};

/* The board's object for the policy, the way a board has it in its data
 * (board.boot_policy, F-265): BOOT_POLICY_AB() stays a constant initializer */
static const struct boot_policy policy = BOOT_POLICY_AB(&cfg_swapped);
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

/* Write a valid state to one copy: the other one's write was cut */
static void write_copy(struct fixture *f,
                       lba_t lba,
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
    memcpy(state_at(f, lba), &s, sizeof(s));
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

/* The candidate the policy hands out, with no result yet: the load runs, or
 * hangs. The test reports it, or doesn't */
static bio_dev_t hand_out(uuid_t uu)
{
    int rc = boot_policy_ab_ops.next(uu);

    return (rc == PB_OK) ? bio_get_part_by_uu(uu) : rc;
}

/*
 * Both copies on the disk are, byte for byte, the state with these fields
 * and nothing else: the reserved bytes and the board's registers zero, the
 * CRC right.
 */
static void assert_disk_state(struct fixture *f,
                              uint32_t enable,
                              uint32_t verified,
                              uint32_t attempts,
                              uint32_t error)
{
    struct pb_boot_state s = { 0 };

    assert_int_equal(sizeof(s), 512);
    s.magic = PB_STATE_MAGIC;
    s.enable = enable;
    s.verified = verified;
    s.remaining_boot_attempts = attempts;
    s.error = error;
    s.crc = crc32(0, (const uint8_t *)&s, sizeof(s));
    assert_memory_equal(state_at(f, PRIMARY_LBA), &s, sizeof(s));
    assert_memory_equal(state_at(f, BACKUP_LBA), &s, sizeof(s));
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

#ifndef CONFIG_BOOT_AB_SPECULATIVE
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
#endif

static void test_init_bad_crc_is_repaired(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    state_at(f, PRIMARY_LBA)->crc ^= 1;

    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
}

/*
 * init reads the state (boot.md 3.1), and a copy that can't be read isn't a
 * corrupt one (F-173). The defaults are for a state that was read and is bad
 * in both copies. With a copy that wasn't read and no good one, init fails
 * with the read's error and writes nothing: the copy may be a good one.
 */
static void test_init_read_error_writes_no_defaults(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);

    /* Neither copy can be read */
    attach(f, true, true);
    f->disk->fail_reads_after = 0;
    assert_int_equal(boot_ab_state_init(&cfg_normal), -PB_ERR_IO);
    assert_int_equal(f->disk->writes, 0);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);

    /* Nothing of a failed read is used (bio.md 3.1), nor what the init
     * before it had: in memory nothing is enabled */
    memset(uu, 0xff, sizeof(uu));
    boot_ab_state_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));

    /* The fault passes: the state is what it was */
    f->disk->fail_reads_after = -1;
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(f->disk->writes, 0);

    /* The primary is corrupt and the backup can't be read: the same. The
     * backup is the good copy here */
    state_at(f, PRIMARY_LBA)->crc ^= 1;
    attach(f, true, true);
    f->disk->fail_reads_after = 1;
    assert_int_equal(boot_ab_state_init(&cfg_normal), -PB_ERR_IO);
    assert_int_equal(f->disk->writes, 0);
    assert_false(state_valid(f, PRIMARY_LBA));
    boot_ab_state_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));

    /* Read again, the primary is repaired from it */
    f->disk->fail_reads_after = -1;
    reboot(f, &cfg_normal);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    assert_int_equal(boot_once(), f->sys_b);
}

/* Partitions as attach() has them, but the primary state on a disk of its
 * own, so that it alone can fail */
static void attach_primary_apart(struct fixture *f, struct ramdisk *primary_disk)
{
    bio_dev_t other;

    attach(f, true, false);
    other = ramdisk_attach(primary_disk, UU_OTHER);
    assert_true(other >= 0);
    assert_true(bio_allocate_parent(other, 0, 0, RAMDISK_BLOCK_SIZE, UU_PRIMARY, "Primary") >= 0);
    assert_true(bio_allocate_parent(bio_get_part_by_uu(UU_DISK),
                                    BACKUP_LBA,
                                    BACKUP_LBA,
                                    RAMDISK_BLOCK_SIZE,
                                    UU_BACKUP,
                                    "Backup") >= 0);
}

/* The same when it is the primary that can't be read */
static void test_init_primary_read_error(void **state)
{
    struct fixture *f = *state;
    struct ramdisk *primary_disk = ramdisk_new(1);
    uuid_t uu;

    assert_non_null(primary_disk);
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    memcpy(ramdisk_block(primary_disk, 0), state_at(f, BACKUP_LBA), RAMDISK_BLOCK_SIZE);

    /* The backup is corrupt: no defaults over a primary that wasn't read */
    state_at(f, BACKUP_LBA)->crc ^= 1;
    attach_primary_apart(f, primary_disk);
    primary_disk->fail_reads_after = 0;
    assert_int_equal(boot_ab_state_init(&cfg_normal), -PB_ERR_IO);
    assert_int_equal(primary_disk->writes, 0);
    assert_int_equal(f->disk->writes, 0);
    boot_ab_state_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));

    /* Read again, it repairs the backup */
    primary_disk->fail_reads_after = -1;
    attach_primary_apart(f, primary_disk);
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_memory_equal(
        state_at(f, BACKUP_LBA), ramdisk_block(primary_disk, 0), RAMDISK_BLOCK_SIZE);
    assert_true(state_valid(f, BACKUP_LBA));
    assert_int_equal(boot_once(), f->sys_b);

    ramdisk_free(primary_disk);
}

/* One copy is good and the other can't be read: the state is the good
 * copy's, which is written to both, as for a copy that is corrupt */
static void test_init_read_error_with_a_good_copy(void **state)
{
    struct fixture *f = *state;
    struct ramdisk *primary_disk = ramdisk_new(1);

    assert_non_null(primary_disk);

    /* The backup can't be read */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    f->disk->fail_reads_after = 1;
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_int_equal(f->disk->writes, 2);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    assert_int_equal(boot_once(), f->sys_b);

    /* The primary can't be read: the backup's state, not what the primary's
     * buffer holds from the init above */
    f->disk->fail_reads_after = -1;
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    attach_primary_apart(f, primary_disk);
    primary_disk->fail_reads_after = 0;
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_int_equal(primary_disk->writes, 1);
    assert_memory_equal(
        ramdisk_block(primary_disk, 0), state_at(f, BACKUP_LBA), RAMDISK_BLOCK_SIZE);
    assert_true(state_valid(f, BACKUP_LBA));
    assert_int_equal(state_at(f, BACKUP_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(boot_once(), f->sys_a);

    ramdisk_free(primary_disk);
}

/*
 * Two valid copies that differ (boot.md 3.7, F-176): a commit, pb's or
 * pbstate's, was cut between its two writes. The primary is written first,
 * so its state is the newer one, and init writes it to both. A backup that
 * stays behind would come back when the primary goes bad.
 */
static void test_init_copies_that_differ_get_the_primarys_state(void **state)
{
    struct fixture *f = *state;
    uint8_t old_backup[RAMDISK_BLOCK_SIZE];
    unsigned int writes;
    uint32_t value = 0;

    /* The OS confirmed B and only the primary got it. The backup has B's
     * trial without attempts left, which rolls back to A */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    write_copy(f, PRIMARY_LBA, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);

    reboot(f, &cfg_normal);
    assert_int_equal(f->disk->writes, 2);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(f->disk->writes, 2);

    /* The primary goes bad: what comes back is the confirmed B, and no
     * rollback to A */
    state_at(f, PRIMARY_LBA)->crc ^= 1;
    reboot(f, &cfg_normal);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
    assert_int_equal(boot_once(), f->sys_b);

    /* Copies that agree: nothing is written */
    writes = f->disk->writes;
    reboot(f, &cfg_normal);
    assert_int_equal(f->disk->writes, writes);

    /* The board's registers are part of the state: a difference in one
     * of them alone */
    assert_int_equal(boot_ab_state_write_board_reg(2, 0x1234), PB_OK);
    memcpy(old_backup, state_at(f, BACKUP_LBA), sizeof(old_backup));
    assert_int_equal(boot_ab_state_write_board_reg(2, 0x5678), PB_OK);
    memcpy(state_at(f, BACKUP_LBA), old_backup, sizeof(old_backup));
    assert_true(state_valid(f, BACKUP_LBA));

    writes = f->disk->writes;
    reboot(f, &cfg_normal);
    assert_int_equal(f->disk->writes, writes + 2);
    assert_memory_equal(state_at(f, BACKUP_LBA), state_at(f, PRIMARY_LBA), RAMDISK_BLOCK_SIZE);
    assert_true(state_valid(f, BACKUP_LBA));
    assert_int_equal(boot_ab_state_read_board_reg(2, &value), PB_OK);
    assert_int_equal(value, 0x5678);
}

/* The backup can't be brought up to date: the state is the primary's and
 * init's result is what it was with two valid copies before F-176, PB_OK.
 * Whether a device with a good primary should stop here is not decided */
static void test_init_copies_that_differ_and_no_write(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    write_copy(f, PRIMARY_LBA, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);

    /* Nothing can be written */
    f->disk->fail_writes_after = 0;
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(state_at(f, BACKUP_LBA)->verified, PB_STATE_A_VERIFIED);

    /* Only the backup can't */
    attach(f, true, true);
    f->disk->fail_writes_after = 1;
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(state_at(f, PRIMARY_LBA)->verified, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED);
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_int_equal(state_at(f, BACKUP_LBA)->verified, PB_STATE_A_VERIFIED);
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

#ifndef CONFIG_BOOT_AB_SPECULATIVE
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
#endif

/*
 * boot.md 3.2 (D-040, F-179): the state write of a decision that fails
 * decides only where skipping it would be unsafe. One test per case.
 *
 * A rollback that can't be written: the verified slot is handed out, and
 * the next boot decides the same again.
 */
static void assert_rollback_without_a_write(struct fixture *f,
                                            uint32_t enable,
                                            uint32_t verified,
                                            bio_dev_t target,
                                            uint32_t enable_after,
                                            uint32_t error_after)
{
    uuid_t uu;

    write_state(f, enable, verified, 0, 0);
    reboot(f, &cfg_normal);

    /* Nothing can be written, as on an eMMC that went read only */
    f->disk->fail_writes_after = 0;
    assert_int_equal(hand_out(uu), target);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    boot_policy_ab_ops.report(uu, PB_OK);
    assert_disk_state(f, enable, verified, 0, 0);

    /* The next boots, with the fault: the same */
    for (unsigned int boot = 0; boot < 2; boot++) {
        attach(f, true, true);
        assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
        assert_int_equal(boot_once(), target);
        assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
        assert_disk_state(f, enable, verified, 0, 0);
    }

    /* The fault passes: the rollback is written */
    f->disk->fail_writes_after = -1;
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), target);
    assert_disk_state(f, enable_after, verified, 0, error_after);
}

static void test_rollback_that_cannot_be_written_is_handed_out(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    assert_rollback_without_a_write(f,
                                    PB_STATE_B_ENABLED,
                                    PB_STATE_A_VERIFIED,
                                    f->sys_a,
                                    PB_STATE_A_ENABLED,
                                    PB_STATE_ERROR_B_ROLLBACK);
    assert_rollback_without_a_write(f,
                                    PB_STATE_A_ENABLED,
                                    PB_STATE_B_VERIFIED,
                                    f->sys_b,
                                    PB_STATE_B_ENABLED,
                                    PB_STATE_ERROR_A_ROLLBACK);

    /* Only the backup can't be written: the same, and the next boot finds
     * the rollback in the primary */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    f->disk->fail_writes_after = 1;
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(state_at(f, BACKUP_LBA)->enable, PB_STATE_B_ENABLED);

    attach(f, true, true);
    f->disk->fail_writes_after = 1;
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_B_ROLLBACK);
}

/*
 * A trial's attempt that can't be written, while the other slot is
 * verified: that slot is handed out instead, mode fallback (3.5, rule 3).
 * pb makes this choice again on every boot with the fault, so an image that
 * verifies and hangs isn't started for ever beside a confirmed system. The
 * trial that wasn't started spent no attempt.
 */
static void assert_no_trial_without_a_write(struct fixture *f,
                                            uint32_t enable,
                                            uint32_t verified,
                                            bio_dev_t trial,
                                            bio_dev_t other)
{
    uuid_t uu;

    write_state(f, enable, verified, 2, 0);
    reboot(f, &cfg_normal);

    f->disk->fail_writes_after = 0;
    assert_int_equal(hand_out(uu), other);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_FALLBACK);
    boot_policy_ab_ops.report(uu, PB_OK);
    assert_disk_state(f, enable, verified, 2, 0);

    /* 'boot status' is the enabled slot (boot.md 3.4): the trial's */
    boot_policy_ab_ops.status(uu);
    assert_int_equal(bio_get_part_by_uu(uu), trial);

    /* Every boot with the fault, more of them than the trial has attempts */
    for (unsigned int boot = 0; boot < 4; boot++) {
        attach(f, true, true);
        assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
        assert_int_equal(boot_once(), other);
        assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_FALLBACK);
        assert_disk_state(f, enable, verified, 2, 0);
    }

    /* The fault passes: the trial is started, with the attempts it had */
    f->disk->fail_writes_after = -1;
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), trial);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_disk_state(f, enable, verified, 1, 0);
}

static void test_trial_attempt_that_cannot_be_written_boots_the_verified_slot(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    assert_no_trial_without_a_write(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, f->sys_b, f->sys_a);
    assert_no_trial_without_a_write(f, PB_STATE_A_ENABLED, PB_STATE_B_VERIFIED, f->sys_a, f->sys_b);

    /* Only the backup can't be written: the same */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, 0);
    reboot(f, &cfg_normal);
    f->disk->fail_writes_after = 1;
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_FALLBACK);
    assert_int_equal(state_at(f, BACKUP_LBA)->remaining_boot_attempts, 2);
}

/* A trial's attempt that can't be written, and nothing else to boot: the
 * trial is handed out uncounted */
static void test_trial_attempt_that_cannot_be_written_and_no_verified_slot(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, 0, 3, 0);
    reboot(f, &cfg_normal);

    f->disk->fail_writes_after = 0;
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    boot_policy_ab_ops.report(uu, PB_OK);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 3, 0);

    /* More boots with the fault than it has attempts: the trial each time */
    for (unsigned int boot = 0; boot < 5; boot++) {
        attach(f, true, true);
        assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
        assert_int_equal(boot_once(), f->sys_a);
        assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
        assert_disk_state(f, PB_STATE_A_ENABLED, 0, 3, 0);
    }

    /* The same from B, on its last attempt */
    f->disk->fail_writes_after = -1;
    write_state(f, PB_STATE_B_ENABLED, 0, 1, 0);
    reboot(f, &cfg_normal);
    f->disk->fail_writes_after = 0;
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 1, 0);
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
}

/*
 * 'boot activate' with a UUID that isn't a slot's changes nothing (boot.md
 * 3.4, F-175): not the state in memory, which 'boot status', the next boot
 * and the next write would act on, nor the state on the disk.
 */
static void test_activate_unknown_uuid_changes_nothing(void **state)
{
    struct fixture *f = *state;
    struct pb_boot_state before;
    uuid_t uu;

    /* A state with every field set: B on trial, A confirmed, an error bit */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, PB_STATE_ERROR_A_ROLLBACK);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_ab_state_write_board_reg(1, 0xb0a4d001), PB_OK);
    memcpy(&before, state_at(f, PRIMARY_LBA), sizeof(before));
    unsigned int writes = f->disk->writes;

    assert_int_equal(boot_policy_ab_ops.activate((unsigned char *)UU_OTHER),
                     -PB_ERR_PART_NOT_BOOTABLE);
    /* A state partition is no slot either */
    assert_int_equal(boot_policy_ab_ops.activate((unsigned char *)UU_PRIMARY),
                     -PB_ERR_PART_NOT_BOOTABLE);
    assert_int_equal(f->disk->writes, writes);

    /* 'boot status' still names the enabled slot */
    boot_policy_ab_ops.status(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);

    /* The next write of the state has what was there: all 512 bytes, but
     * the register it writes and the CRC */
    assert_int_equal(boot_ab_state_write_board_reg(0, 0xb0a4d000), PB_OK);
    before.board_regs[PB_STATE_NO_OF_BOARD_REGS - 1] = 0xb0a4d000;
    before.crc = 0;
    before.crc = crc32(0, (const uint8_t *)&before, sizeof(before));
    assert_memory_equal(state_at(f, PRIMARY_LBA), &before, sizeof(before));
    assert_memory_equal(state_at(f, BACKUP_LBA), &before, sizeof(before));

    /* And the next boot is the trial's, counted */
    assert_int_equal(boot_once(), f->sys_b);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_B_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->verified, PB_STATE_A_VERIFIED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 1);
    assert_int_equal(state_at(f, PRIMARY_LBA)->error, PB_STATE_ERROR_A_ROLLBACK);
}

static void test_part_names(void **state)
{
    struct fixture *f = *state;

    reboot(f, &cfg_normal);
    assert_string_equal(boot_ab_part_uu_to_name((unsigned char *)UU_SYS_A), "A");
    assert_string_equal(boot_ab_part_uu_to_name((unsigned char *)UU_SYS_B), "B");
    assert_string_equal(boot_ab_part_uu_to_name((unsigned char *)UU_OTHER), "?");

    /* They are the policy's names for the handover (boot.md 5.6) */
    assert_string_equal(boot_policy_ab_ops.system_name((unsigned char *)UU_SYS_B), "B");
    assert_string_equal(boot_policy_ab_ops.system_name((unsigned char *)UU_OTHER), "?");
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
    assert_ptr_equal(policy.config, &cfg_swapped);

    /* The module has the other configuration, so that what next() does below
     * shows that the object's reached it */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);

    /* An error of the module's is init's answer */
    attach(f, false, true);
    assert_int_equal(policy.ops->init(&policy), -PB_ERR_NOT_FOUND);

    attach(f, true, true);
    assert_int_equal(policy.ops->init(&policy), PB_OK);

    /* The state's slot B is the partition System A, only with the object's
     * configuration */
    assert_int_equal(boot_once(), f->sys_a);
    policy.ops->status(uu);
    assert_memory_equal(uu, UU_SYS_A, 16);

    /* A host activates the partition System B: the state's slot A */
    assert_int_equal(policy.ops->activate((unsigned char *)UU_SYS_B), PB_OK);
    assert_int_equal(boot_once(), f->sys_b);
    policy.ops->status(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);
    assert_int_equal(state_at(f, PRIMARY_LBA)->verified, PB_STATE_A_VERIFIED);

    /* A partition that isn't a system: refused, and System B stays active
     * (boot.md 3.4) */
    assert_int_equal(policy.ops->activate((unsigned char *)UU_OTHER), -PB_ERR_PART_NOT_BOOTABLE);
    policy.ops->status(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);
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

#ifndef CONFIG_BOOT_AB_SPECULATIVE
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
#endif

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

#ifndef CONFIG_BOOT_AB_SPECULATIVE
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

/*
 * Without CONFIG_BOOT_AB_SPECULATIVE there is no speculative mode, and the
 * board's data has nothing that could ask for one (boot.md 3.6: Kconfig is
 * the only switch). Each of its three starting cases ends without a
 * candidate, nothing is written, and the mode never reads speculative.
 */
static void test_no_speculative_mode_without_the_symbol(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    /* From the state: a trial without attempts left, the other slot not
     * confirmed */
    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    write_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);

    /* From this boot: the enabled slot fails, the other isn't confirmed */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);

    /* Nothing enabled: a blank state */
    write_state(f, 0, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_disk_state(f, 0, 0, 0, 0);
    assert_int_equal(f->disk->writes, 0);

    /* Nothing enabled: disabled by the host, which parks the device in
     * command mode */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    uuid_clear(uu);
    assert_int_equal(boot_policy_ab_ops.activate(uu), PB_OK);
    assert_int_equal(f->disk->writes, 2);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(f->disk->writes, 2);
    assert_disk_state(f, 0, 0, 0, 0);
}
#endif

/*
 * What A/B writes where it has a viable candidate, byte for byte, the same
 * with and without speculative mode: the defaults, a trial's attempt, a
 * rollback, 'boot activate'. The format is fixed (boot.md 3.7 and 7).
 */
static void test_writes_with_a_viable_candidate(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;
    unsigned int writes;

    /* A blank disk: the defaults */
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_disk_state(f, 0, 0, 0, 0);

    /* A confirmed slot: nothing is written */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    writes = f->disk->writes;
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(f->disk->writes, writes);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);

    /* A trial: one attempt less, written before the load */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 1, 0);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    boot_policy_ab_ops.report(uu, PB_OK);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 1, 0);

    /* A trial without attempts left and a confirmed slot: the rollback */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, PB_STATE_ERROR_B_ROLLBACK);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    boot_policy_ab_ops.report(uu, PB_OK);

    write_state(f, PB_STATE_A_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, PB_STATE_ERROR_A_ROLLBACK);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    boot_policy_ab_ops.report(uu, PB_OK);

    /* 'boot activate': the slot enabled and verified alone, no error bits.
     * With the null UUID nothing is */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, PB_STATE_ERROR_A_ROLLBACK);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_policy_ab_ops.activate((unsigned char *)UU_SYS_A), PB_OK);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    uuid_clear(uu);
    assert_int_equal(boot_policy_ab_ops.activate(uu), PB_OK);
    assert_disk_state(f, 0, 0, 0, 0);
}

/*
 * A rollback with the other slot's bit already set (boot.md 3.5, rule 2, F-340):
 * the rolled back slot's bit is set and the other one's stays. All 512 bytes
 * of both copies: the state before with the other slot enabled and the bit
 * set, the board's registers kept, the CRC over that.
 */
static void assert_rollback_keeps_the_bit(struct fixture *f,
                                          uint32_t enable,
                                          uint32_t verified,
                                          uint32_t error_before,
                                          bio_dev_t target,
                                          uint32_t enable_after,
                                          uint32_t error_after)
{
    struct pb_boot_state expected;
    uuid_t uu;

    write_state(f, enable, verified, 0, error_before);
    reboot(f, &cfg_normal);
    for (unsigned int i = 0; i < PB_STATE_NO_OF_BOARD_REGS; i++)
        assert_int_equal(boot_ab_state_write_board_reg(i, 0xb0a4d000 + i), PB_OK);
    memcpy(&expected, state_at(f, PRIMARY_LBA), sizeof(expected));
    reboot(f, &cfg_normal);

    assert_int_equal(hand_out(uu), target);
    assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_NORMAL);
    boot_policy_ab_ops.report(uu, PB_OK);

    expected.enable = enable_after;
    expected.error = error_after;
    expected.crc = 0;
    expected.crc = crc32(0, (const uint8_t *)&expected, sizeof(expected));
    assert_memory_equal(state_at(f, PRIMARY_LBA), &expected, sizeof(expected));
    assert_memory_equal(state_at(f, BACKUP_LBA), &expected, sizeof(expected));
}

static void test_rollback_keeps_the_other_slots_bit(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    /* From A, with B's bit set: both bits */
    assert_rollback_keeps_the_bit(f,
                                  PB_STATE_A_ENABLED,
                                  PB_STATE_B_VERIFIED,
                                  PB_STATE_ERROR_B_ROLLBACK,
                                  f->sys_b,
                                  PB_STATE_B_ENABLED,
                                  PB_STATE_ERROR_A_ROLLBACK | PB_STATE_ERROR_B_ROLLBACK);

    /* The mirror: from B, with A's bit set */
    assert_rollback_keeps_the_bit(f,
                                  PB_STATE_B_ENABLED,
                                  PB_STATE_A_VERIFIED,
                                  PB_STATE_ERROR_A_ROLLBACK,
                                  f->sys_a,
                                  PB_STATE_A_ENABLED,
                                  PB_STATE_ERROR_A_ROLLBACK | PB_STATE_ERROR_B_ROLLBACK);

    /* The rolled back slot's own bit already set: it stays, alone */
    assert_rollback_keeps_the_bit(f,
                                  PB_STATE_A_ENABLED,
                                  PB_STATE_B_VERIFIED,
                                  PB_STATE_ERROR_A_ROLLBACK,
                                  f->sys_b,
                                  PB_STATE_B_ENABLED,
                                  PB_STATE_ERROR_A_ROLLBACK);

    /* No bit set: the bytes every released pb wrote (D-008), its own bit
     * alone, the rest zero */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, PB_STATE_ERROR_A_ROLLBACK);

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, PB_STATE_ERROR_B_ROLLBACK);
}

#ifdef CONFIG_BOOT_AB_SPECULATIVE
/*
 * Speculative mode (boot.md 3.6), with CONFIG_BOOT_AB_SPECULATIVE. One test
 * per rule; rule 6, the watchdog dependency, is Kconfig's and has its test
 * in check_ab_speculative_kconfig.py.
 */

/*
 * The OS confirms a slot, as pbstate does (pbstate_set_system_verified() in
 * tools/pbstate): the slot's verified bit is set and the attempts are zeroed.
 * The enabled slot and the error bits stay. Both copies are written.
 */
static void pbstate_confirms(struct fixture *f, uint32_t verified_bit)
{
    struct pb_boot_state s;

    assert_true(state_valid(f, PRIMARY_LBA));
    memcpy(&s, state_at(f, PRIMARY_LBA), sizeof(s));
    s.verified |= verified_bit;
    s.remaining_boot_attempts = 0;
    s.crc = 0;
    s.crc = crc32(0, (const uint8_t *)&s, sizeof(s));
    memcpy(state_at(f, PRIMARY_LBA), &s, sizeof(s));
    memcpy(state_at(f, BACKUP_LBA), &s, sizeof(s));
}

/*
 * The OS invalidates a slot, as pbstate does (pbstate_invalidate_system()):
 * the slot's enable and verified bits are cleared. The error bits stay.
 */
static void pbstate_invalidates(struct fixture *f, uint32_t enable_bit, uint32_t verified_bit)
{
    struct pb_boot_state s;

    assert_true(state_valid(f, PRIMARY_LBA));
    memcpy(&s, state_at(f, PRIMARY_LBA), sizeof(s));
    s.enable &= ~enable_bit;
    s.verified &= ~verified_bit;
    s.crc = 0;
    s.crc = crc32(0, (const uint8_t *)&s, sizeof(s));
    memcpy(state_at(f, PRIMARY_LBA), &s, sizeof(s));
    memcpy(state_at(f, BACKUP_LBA), &s, sizeof(s));
}

#define assert_mode(m) assert_int_equal(boot_policy_ab_ops.mode(), BOOT_POLICY_MODE_##m)

/* Rule 1, from the state: the enabled slot is a trial without attempts left
 * and the other slot isn't confirmed. The other slot is enabled, written and
 * handed out */
static void test_speculative_starts_from_the_state(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    assert_int_equal(f->disk->writes, 2);
    boot_policy_ab_ops.report(uu, PB_OK);
    /* 'boot status': the enabled slot, which is the one that booted */
    boot_policy_ab_ops.status(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);

    write_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    boot_policy_ab_ops.report(uu, PB_OK);

    /* Not this case, the other slot is confirmed: the rollback of 3.5, rule
     * 2, which is no speculative boot */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(NORMAL);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, PB_STATE_ERROR_A_ROLLBACK);

    /* Nor this, a trial with an attempt left: it is tried, and counted */
    write_state(f, PB_STATE_A_ENABLED, 0, 1, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    /* Its last attempt is spent: on the next boot the state is the case */
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
}

/* Rule 1, from this boot: the candidates of 3.5 failed and a slot that isn't
 * confirmed is left. It is tried for this boot only, with nothing written:
 * the enabled slot keeps its state and is first again on the next boot */
static void test_speculative_starts_from_this_boot(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;
    unsigned int writes;

    /* What 'boot activate A' leaves: A confirmed, B not. A fails */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_mode(NORMAL);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_mode(SPECULATIVE);
    boot_policy_ab_ops.report(uu, PB_OK);
    assert_mode(SPECULATIVE);

    assert_int_equal(f->disk->writes, 0);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    boot_policy_ab_ops.status(uu);
    assert_memory_equal(uu, UU_SYS_A, 16);

    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);
    assert_int_equal(f->disk->writes, 0);

    /* A trial that fails spent its attempt, as without the mode. The slot
     * that isn't confirmed is tried, and nothing more is written */
    write_state(f, PB_STATE_B_ENABLED, 0, 2, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_mode(NORMAL);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 1, 0);
    writes = f->disk->writes;
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_int_equal(f->disk->writes, writes);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 1, 0);

    /* The next boot: the trial again, its last attempt */
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);

    /* The slot a rollback handed out fails: the trial that ran out is the
     * slot that is left */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_mode(NORMAL);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, PB_STATE_ERROR_B_ROLLBACK);
    writes = f->disk->writes;
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(SPECULATIVE);
    assert_int_equal(f->disk->writes, writes);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, PB_STATE_ERROR_B_ROLLBACK);

    /* Not this case, the other slot is confirmed: the fallback of 3.5, rule
     * 3. When it fails too, no slot is left */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_mode(FALLBACK);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
}

/* Rule 1, nothing enabled: a state that is blank, lost or disabled by the
 * host. A is tried first (D-040), then it alternates */
static void test_speculative_starts_with_nothing_enabled(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    /* A blank disk: init installs the defaults */
    assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
    assert_disk_state(f, 0, 0, 0, 0);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    boot_policy_ab_ops.report(uu, PB_OK);

    /* From here the state is rule 1's first case: B, A */
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(SPECULATIVE);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);

    /* A state that was lost: neither copy is valid */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    state_at(f, PRIMARY_LBA)->crc ^= 1;
    state_at(f, BACKUP_LBA)->magic = 0;
    reboot(f, &cfg_normal);
    assert_disk_state(f, 0, 0, 0, 0);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    /* Disabled by the host: 'boot activate' with the null UUID doesn't
     * park the device in command mode */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    uuid_clear(uu);
    assert_int_equal(boot_policy_ab_ops.activate(uu), PB_OK);
    assert_disk_state(f, 0, 0, 0, 0);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    /* Disabled by the OS, which keeps what was confirmed ('pbstate -s
     * none'): A first all the same. The state then decides as ever: a slot
     * that is confirmed boots without the mode, one that isn't is rolled
     * back to the confirmed one */
    write_state(f, 0, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);

    write_state(f, 0, PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_B_VERIFIED, 0, 0);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(NORMAL);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, PB_STATE_ERROR_A_ROLLBACK);
}

/*
 * Rule 2: never an image that fails verification. The policy only hands out
 * candidates: the core verifies each one (boot.md 4.2) and reports it. One
 * that failed isn't kept: it isn't handed out again in that boot, the next
 * boot starts with the other slot, and when both failed there is none, so pb
 * enters command mode. Only the two slots are ever handed out, and the
 * policy never marks one confirmed.
 */
static void test_speculative_never_keeps_a_candidate_that_fails(void **state)
{
    struct fixture *f = *state;
    uuid_t first, second, none;
    bio_dev_t expect_first, expect_second;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    for (int boot = 0; boot < 4; boot++) {
        reboot(f, &cfg_normal);
        expect_first = (boot % 2 == 0) ? f->sys_b : f->sys_a;
        expect_second = (boot % 2 == 0) ? f->sys_a : f->sys_b;

        assert_int_equal(hand_out(first), expect_first);
        assert_true(bio_get_flags(expect_first) & BIO_FLAG_BOOTABLE);
        boot_policy_ab_ops.report(first, -PB_ERR_SIGNATURE);

        assert_int_equal(hand_out(second), expect_second);
        assert_true(bio_get_flags(expect_second) & BIO_FLAG_BOOTABLE);
        boot_policy_ab_ops.report(second, -PB_ERR_BAD_PAYLOAD);

        assert_int_equal(boot_policy_ab_ops.next(none), -PB_ERR_NOT_FOUND);
        assert_int_equal(state_at(f, PRIMARY_LBA)->verified, 0);
        assert_int_equal(state_at(f, BACKUP_LBA)->verified, 0);
    }

    /* The same when it started in this boot */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(boot_policy_ab_ops.next(none), -PB_ERR_NOT_FOUND);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);

    /* And with nothing enabled, as a device in the factory with two empty
     * slots: command mode is still reached */
    write_state(f, 0, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(boot_policy_ab_ops.next(none), -PB_ERR_NOT_FOUND);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
}

/* Rule 3: it alternates per boot. The other slot is enabled and written
 * before the load: both copies hold it before any result is reported, and
 * the report writes nothing */
static void test_speculative_alternates_per_boot(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    for (unsigned int boot = 0; boot < 6; boot++) {
        bool b_turn = (boot % 2 == 0);
        unsigned int writes;

        reboot(f, &cfg_normal);
        writes = f->disk->writes;

        assert_int_equal(hand_out(uu), b_turn ? f->sys_b : f->sys_a);
        assert_disk_state(f, b_turn ? PB_STATE_B_ENABLED : PB_STATE_A_ENABLED, 0, 0, 0);
        assert_int_equal(f->disk->writes, writes + 2);
        assert_mode(SPECULATIVE);

        boot_policy_ab_ops.report(uu, PB_OK);
        assert_int_equal(f->disk->writes, writes + 2);
        assert_disk_state(f, b_turn ? PB_STATE_B_ENABLED : PB_STATE_A_ENABLED, 0, 0, 0);
    }
}

/* Rule 3: a hang and the watchdog lead to the other slot. The load never
 * comes back, so nothing is reported, and the reset finds what was written */
static void test_speculative_hang_leads_to_the_other_slot(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_b);

    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_mode(SPECULATIVE);

    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);

    /* The same with nothing enabled: A hangs, B is next */
    write_state(f, 0, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_a);
    reboot(f, &cfg_normal);
    assert_int_equal(hand_out(uu), f->sys_b);
}

/* Rule 3: in the same boot, when its slot fails, the other is tried,
 * without another write */
static void test_speculative_same_boot_tries_the_other_without_a_write(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(f->disk->writes, 2);

    assert_int_equal(hand_out(uu), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_int_equal(f->disk->writes, 2);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    boot_policy_ab_ops.report(uu, PB_OK);
    assert_int_equal(f->disk->writes, 2);

    /* 'boot status' is the enabled slot (boot.md 3.4): B, while A runs */
    boot_policy_ab_ops.status(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);

    /* The next boot alternates from what was written: A first */
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    assert_int_equal(f->disk->writes, 4);

    /* Both fail: none, and still one write per copy for the boot. A boot
     * that command mode starts later counts like any boot (boot.md 3.2) */
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_int_equal(boot_policy_ab_ops.next(uu), -PB_ERR_NOT_FOUND);
    assert_int_equal(f->disk->writes, 6);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    assert_int_equal(boot_once(), f->sys_a);
    assert_int_equal(f->disk->writes, 8);
}

/* Rule 4: it counts no attempts. A slot gets one boot, not the two of the
 * released mode (F-177: B, B, A), and the counter stays zero */
static void test_speculative_counts_no_attempts(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);

    for (unsigned int boot = 0; boot < 6; boot++) {
        assert_int_equal(boot_once(), (boot % 2 == 0) ? f->sys_b : f->sys_a);
        assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 0);
        assert_int_equal(state_at(f, BACKUP_LBA)->remaining_boot_attempts, 0);
    }

    /* Nor when both slots fail */
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);

    /* A counter that was left behind with nothing enabled ('boot activate'
     * with the null UUID keeps it, F-179) isn't counted either: the slot
     * the mode enables gets one boot */
    write_state(f, 0, 0, 3, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    assert_int_equal(boot_once(), f->sys_b);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
}

/* Rule 4: it ends when the OS confirms a slot through pbstate */
static void test_speculative_ends_when_the_os_confirms(void **state)
{
    struct fixture *f = *state;
    unsigned int writes;

    /* From the state: B runs, and the OS confirms it */
    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(SPECULATIVE);
    pbstate_confirms(f, PB_STATE_B_VERIFIED);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);

    writes = f->disk->writes;
    for (unsigned int boot = 0; boot < 3; boot++) {
        reboot(f, &cfg_normal);
        assert_int_equal(boot_once(), f->sys_b);
        assert_mode(NORMAL);
    }
    assert_int_equal(f->disk->writes, writes);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_B_VERIFIED, 0, 0);

    /* Nothing enabled: A runs, and the OS confirms it */
    write_state(f, 0, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    pbstate_confirms(f, PB_STATE_A_VERIFIED);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);

    /* The slot that was written failed and the other one booted in the same
     * boot. The OS confirms the one it runs in, A, while B is the enabled
     * one: the next boot rolls back to A (3.5, rule 2), and stays there */
    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_b);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    pbstate_confirms(f, PB_STATE_A_VERIFIED);
    assert_disk_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, PB_STATE_ERROR_B_ROLLBACK);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);

    /* From this boot: A is confirmed and fails, B runs and the OS confirms
     * it. When A fails again, B is the fallback of 3.5, rule 3 */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(SPECULATIVE);
    pbstate_confirms(f, PB_STATE_B_VERIFIED);
    reboot(f, &cfg_normal);
    assert_int_equal(candidate_fails(), f->sys_a);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(FALLBACK);
}

/* Rule 5: the mode reads speculative for a candidate that speculative mode
 * handed out, through its report, and for no other */
static void test_speculative_mode_is_reported(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    /* A confirmed slot */
    write_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_mode(NORMAL);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_mode(NORMAL);
    /* The confirmed fallback */
    boot_policy_ab_ops.report(uu, -PB_ERR_BAD_PAYLOAD);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_mode(FALLBACK);
    boot_policy_ab_ops.report(uu, PB_OK);

    /* A trial with attempts, then the slot that isn't confirmed in the same
     * boot: from this boot */
    write_state(f, PB_STATE_A_ENABLED, 0, 1, 0);
    reboot(f, &cfg_normal);
    assert_mode(NORMAL);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_mode(NORMAL);
    boot_policy_ab_ops.report(uu, -PB_ERR_BAD_PAYLOAD);
    assert_mode(NORMAL);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_mode(SPECULATIVE);
    boot_policy_ab_ops.report(uu, PB_OK);
    /* The handover reads it after the report */
    assert_mode(SPECULATIVE);

    /* The next boot, from the state: its first candidate and the other one */
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_mode(SPECULATIVE);
    boot_policy_ab_ops.report(uu, -PB_ERR_BAD_PAYLOAD);
    assert_mode(SPECULATIVE);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_mode(SPECULATIVE);
    boot_policy_ab_ops.report(uu, PB_OK);
    assert_mode(SPECULATIVE);

    /* The OS confirms A, which B's failure left running: the rollback to
     * it and every boot after are normal */
    pbstate_confirms(f, PB_STATE_A_VERIFIED);
    reboot(f, &cfg_normal);
    assert_mode(NORMAL);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);

    /* A rollback is normal, a slot the host activated is normal */
    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);
    assert_int_equal(boot_policy_ab_ops.activate((unsigned char *)UU_SYS_B), PB_OK);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(NORMAL);

    /* Nothing enabled: speculative. And a new set up starts normal */
    uuid_clear(uu);
    assert_int_equal(boot_policy_ab_ops.activate(uu), PB_OK);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    reboot(f, &cfg_normal);
    assert_mode(NORMAL);
}

/*
 * The history that reaches a rollback with the other slot's bit set (F-340):
 * B's trial rolls back to A (B's bit), the OS confirms B and invalidates A,
 * speculative mode boots A, which isn't confirmed, and the next boot rolls
 * back to B. A's bit is set and B's stays (boot.md 3.5, rule 2).
 */
static void test_speculative_then_rollback_keeps_both_bits(void **state)
{
    struct fixture *f = *state;

    write_state(f, PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 1, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(NORMAL);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, PB_STATE_ERROR_B_ROLLBACK);

    pbstate_confirms(f, PB_STATE_B_VERIFIED);
    pbstate_invalidates(f, PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED);
    assert_disk_state(f, 0, PB_STATE_B_VERIFIED, 0, PB_STATE_ERROR_B_ROLLBACK);

    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, PB_STATE_B_VERIFIED, 0, PB_STATE_ERROR_B_ROLLBACK);

    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(NORMAL);
    assert_disk_state(f,
                      PB_STATE_B_ENABLED,
                      PB_STATE_B_VERIFIED,
                      0,
                      PB_STATE_ERROR_A_ROLLBACK | PB_STATE_ERROR_B_ROLLBACK);
}

/*
 * What the mode's write must leave, all 512 bytes of both copies: the state
 * 'before' with 'enable', the attempts zero and the CRC over that.
 */
static void assert_only_enable_written(struct fixture *f,
                                       const struct pb_boot_state *before,
                                       uint32_t enable)
{
    struct pb_boot_state expected;

    memcpy(&expected, before, sizeof(expected));
    expected.enable = enable;
    expected.remaining_boot_attempts = 0;
    expected.crc = 0;
    expected.crc = crc32(0, (const uint8_t *)&expected, sizeof(expected));
    assert_memory_equal(state_at(f, PRIMARY_LBA), &expected, sizeof(expected));
    assert_memory_equal(state_at(f, BACKUP_LBA), &expected, sizeof(expected));
}

/*
 * Rule 7: no format change. The alternation is 'enable': the board's
 * registers, the error bits (F-177: the released mode dropped one), the
 * verified bits and the reserved bytes are as they were, byte for byte. The
 * one other field the mode writes is the attempts, zeroed, which rule 4
 * needs when a counter was left behind with nothing enabled; from the state
 * they are zero already. The case that starts in a boot writes nothing at
 * all (test_speculative_starts_from_this_boot).
 */
static void test_speculative_writes_only_enable(void **state)
{
    struct fixture *f = *state;
    struct pb_boot_state before;

    /* From the state */
    write_state(f, PB_STATE_A_ENABLED, 0, 0, PB_STATE_ERROR_A_ROLLBACK | PB_STATE_ERROR_B_ROLLBACK);
    reboot(f, &cfg_normal);
    for (unsigned int i = 0; i < PB_STATE_NO_OF_BOARD_REGS; i++)
        assert_int_equal(boot_ab_state_write_board_reg(i, 0xb0a4d000 + i), PB_OK);

    for (unsigned int boot = 0; boot < 4; boot++) {
        bool b_turn = (boot % 2 == 0);

        memcpy(&before, state_at(f, PRIMARY_LBA), sizeof(before));
        assert_int_equal(before.enable, b_turn ? PB_STATE_A_ENABLED : PB_STATE_B_ENABLED);
        assert_int_equal(before.error, PB_STATE_ERROR_A_ROLLBACK | PB_STATE_ERROR_B_ROLLBACK);
        assert_int_equal(before.board_regs[0], 0xb0a4d003);

        reboot(f, &cfg_normal);
        assert_int_equal(boot_once(), b_turn ? f->sys_b : f->sys_a);
        assert_only_enable_written(f, &before, b_turn ? PB_STATE_B_ENABLED : PB_STATE_A_ENABLED);
    }

    /* Nothing enabled, with attempts left behind, a slot confirmed, the
     * error bits and the board's registers: 'enable' and the attempts */
    write_state(
        f, 0, PB_STATE_B_VERIFIED, 3, PB_STATE_ERROR_A_ROLLBACK | PB_STATE_ERROR_B_ROLLBACK);
    reboot(f, &cfg_normal);
    for (unsigned int i = 0; i < PB_STATE_NO_OF_BOARD_REGS; i++)
        assert_int_equal(boot_ab_state_write_board_reg(i, 0xb0a4d000 + i), PB_OK);

    memcpy(&before, state_at(f, PRIMARY_LBA), sizeof(before));
    assert_int_equal(before.enable, 0);
    assert_int_equal(before.remaining_boot_attempts, 3);
    assert_int_equal(before.verified, PB_STATE_B_VERIFIED);

    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_only_enable_written(f, &before, PB_STATE_A_ENABLED);
    assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 0);
    assert_int_equal(state_at(f, BACKUP_LBA)->remaining_boot_attempts, 0);
    assert_int_equal(state_at(f, PRIMARY_LBA)->verified, PB_STATE_B_VERIFIED);
}

/*
 * boot.md 3.2 (D-040, F-179): speculative mode's alternation that can't be
 * written: the slot is handed out. A hang then comes back to the same
 * slot, which is still better than no system.
 */
static void test_speculative_alternation_that_cannot_be_written_is_handed_out(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    /* From the state */
    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);

    f->disk->fail_writes_after = 0;
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    /* It hangs: the reset finds the state as it was, and B again */
    for (unsigned int boot = 0; boot < 3; boot++) {
        attach(f, true, true);
        assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
        assert_int_equal(hand_out(uu), f->sys_b);
        assert_mode(SPECULATIVE);
        assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    }

    /* In the same boot, when its slot fails, the other is tried (3.6, rule
     * 3), which needs no write */
    boot_policy_ab_ops.report(uu, -PB_ERR_BAD_PAYLOAD);
    assert_int_equal(hand_out(uu), f->sys_a);
    assert_mode(SPECULATIVE);
    boot_policy_ab_ops.report(uu, PB_OK);

    /* Nothing enabled: A, and A again */
    f->disk->fail_writes_after = -1;
    write_state(f, 0, 0, 0, 0);
    reboot(f, &cfg_normal);
    f->disk->fail_writes_after = 0;

    for (unsigned int boot = 0; boot < 2; boot++) {
        attach(f, true, true);
        assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);
        assert_int_equal(hand_out(uu), f->sys_a);
        assert_mode(SPECULATIVE);
        assert_disk_state(f, 0, 0, 0, 0);
    }

    /* The fault passes: the alternation is written again */
    f->disk->fail_writes_after = -1;
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
}

/*
 * A backup that takes no write, for good: the alternation's second write
 * fails on every boot, and init can't bring the backup up to date either.
 * Each boot hands its slot out all the same (boot.md 3.2), and since the
 * primary takes the write the slots still alternate. Before F-179 the
 * primary alternated and nothing booted.
 */
static void test_speculative_backup_that_cannot_be_written(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    for (unsigned int boot = 0; boot < 6; boot++) {
        bool b_turn = (boot % 2 == 0);

        attach(f, true, true);
        f->disk->fail_writes_after = 1;
        assert_int_equal(boot_ab_state_init(&cfg_normal), PB_OK);

        f->disk->fail_writes_after = 1;
        assert_int_equal(hand_out(uu), b_turn ? f->sys_b : f->sys_a);
        assert_mode(SPECULATIVE);
        boot_policy_ab_ops.report(uu, PB_OK);

        assert_true(state_valid(f, PRIMARY_LBA));
        assert_true(state_valid(f, BACKUP_LBA));
        assert_int_equal(state_at(f, PRIMARY_LBA)->enable,
                         b_turn ? PB_STATE_B_ENABLED : PB_STATE_A_ENABLED);
        assert_int_equal(state_at(f, BACKUP_LBA)->enable, PB_STATE_A_ENABLED);
        assert_int_equal(state_at(f, PRIMARY_LBA)->remaining_boot_attempts, 0);

        /* 'boot status': the enabled slot, the one that booted */
        boot_policy_ab_ops.status(uu);
        assert_int_equal(bio_get_part_by_uu(uu), b_turn ? f->sys_b : f->sys_a);
    }
}

/*
 * Power is lost between the two copies' writes, or in one of them. The
 * alternation holds and no slot gets stuck: the boot that was cut never
 * loaded its slot, the next one goes to a slot from what init finds, and
 * the copies agree again after it.
 */
static void test_speculative_power_loss_between_the_copies(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;
    unsigned int writes;

    write_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);

    /* The next boot enables A, and power is lost after the primary's
     * write: the backup still has B, and A was never loaded. Here the
     * backup's write fails and the candidate is never loaded or reported */
    reboot(f, &cfg_normal);
    f->disk->fail_writes_after = 1;
    assert_int_equal(hand_out(uu), f->sys_a);
    f->disk->fail_writes_after = -1;
    assert_true(state_valid(f, PRIMARY_LBA));
    assert_true(state_valid(f, BACKUP_LBA));
    assert_int_equal(state_at(f, PRIMARY_LBA)->enable, PB_STATE_A_ENABLED);
    assert_int_equal(state_at(f, BACKUP_LBA)->enable, PB_STATE_B_ENABLED);

    /* Both copies are valid and differ: the primary wins at init, which
     * writes it to both (F-176). A's turn was cut, so this boot is B's */
    writes = f->disk->writes;
    reboot(f, &cfg_normal);
    assert_int_equal(f->disk->writes, writes + 2);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    assert_int_equal(hand_out(uu), f->sys_b);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    boot_policy_ab_ops.report(uu, PB_OK);

    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);

    /* A torn primary: power is lost in its write. Init restores it from
     * the backup, which has the boot before's slot, B: this boot is A's */
    memset(state_at(f, PRIMARY_LBA), 0xa5, 100);
    assert_false(state_valid(f, PRIMARY_LBA));
    reboot(f, &cfg_normal);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);
    assert_int_equal(boot_once(), f->sys_a);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);

    /* A torn backup: the primary has this boot's slot, A, and init writes
     * it to the backup. The slot was written, so the next boot is B's */
    memset(state_at(f, BACKUP_LBA), 0xa5, 100);
    assert_false(state_valid(f, BACKUP_LBA));
    reboot(f, &cfg_normal);
    assert_disk_state(f, PB_STATE_A_ENABLED, 0, 0, 0);
    assert_int_equal(boot_once(), f->sys_b);
    assert_mode(SPECULATIVE);
    assert_disk_state(f, PB_STATE_B_ENABLED, 0, 0, 0);

    /* And on it goes */
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_a);
    reboot(f, &cfg_normal);
    assert_int_equal(boot_once(), f->sys_b);
}
#endif

#define AB_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        AB_TEST(test_not_initialized),
        AB_TEST(test_init_requires_system_partitions),
        AB_TEST(test_init_without_state_partitions_fails),
#ifndef CONFIG_BOOT_AB_SPECULATIVE
        AB_TEST(test_init_blank_installs_defaults),
#endif
        AB_TEST(test_init_bad_crc_is_repaired),
        AB_TEST(test_init_read_error_writes_no_defaults),
        AB_TEST(test_init_primary_read_error),
        AB_TEST(test_init_read_error_with_a_good_copy),
        AB_TEST(test_init_copies_that_differ_get_the_primarys_state),
        AB_TEST(test_init_copies_that_differ_and_no_write),
        AB_TEST(test_commit_write_errors),
        AB_TEST(test_commit_erases_first),
        AB_TEST(test_verified_system_boots),
        AB_TEST(test_b_boot_attempts),
#ifndef CONFIG_BOOT_AB_SPECULATIVE
        AB_TEST(test_no_rollback_to_unverified_system),
#endif
        AB_TEST(test_rollback_that_cannot_be_written_is_handed_out),
        AB_TEST(test_trial_attempt_that_cannot_be_written_boots_the_verified_slot),
        AB_TEST(test_trial_attempt_that_cannot_be_written_and_no_verified_slot),
        AB_TEST(test_set_boot_partition),
        AB_TEST(test_activate_unknown_uuid_changes_nothing),
        AB_TEST(test_part_names),
        AB_TEST(test_board_regs),
        AB_TEST(test_policy_object),
        AB_TEST(test_init_marks_the_candidates),
        AB_TEST(test_fallback_to_the_verified_slot),
        AB_TEST(test_fallback_fails_too),
        AB_TEST(test_trial_that_fails_falls_back),
        AB_TEST(test_writes_with_a_viable_candidate),
        AB_TEST(test_rollback_keeps_the_other_slots_bit),
#ifndef CONFIG_BOOT_AB_SPECULATIVE
        /* Without a viable candidate there is none */
        AB_TEST(test_no_fallback_to_unverified_slot),
        AB_TEST(test_trial_that_fails_without_a_verified_slot),
        AB_TEST(test_rollback_target_fails),
        AB_TEST(test_no_speculative_mode_without_the_symbol),
#else
        /* Without a viable candidate: speculative mode (boot.md 3.6) */
        AB_TEST(test_speculative_starts_from_the_state),
        AB_TEST(test_speculative_starts_from_this_boot),
        AB_TEST(test_speculative_starts_with_nothing_enabled),
        AB_TEST(test_speculative_never_keeps_a_candidate_that_fails),
        AB_TEST(test_speculative_alternates_per_boot),
        AB_TEST(test_speculative_hang_leads_to_the_other_slot),
        AB_TEST(test_speculative_same_boot_tries_the_other_without_a_write),
        AB_TEST(test_speculative_counts_no_attempts),
        AB_TEST(test_speculative_ends_when_the_os_confirms),
        AB_TEST(test_speculative_mode_is_reported),
        AB_TEST(test_speculative_then_rollback_keeps_both_bits),
        AB_TEST(test_speculative_writes_only_enable),
        AB_TEST(test_speculative_alternation_that_cannot_be_written_is_handed_out),
        AB_TEST(test_speculative_backup_that_cannot_be_written),
        AB_TEST(test_speculative_power_loss_between_the_copies),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
