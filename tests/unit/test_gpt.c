/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/partition/gpt.h>
#include <pb/bio.h>
#include <pb/crc.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/ramdisk.h"

/* 32 MiB disk, same as the host and QEMU test boards */
#define DISK_BLOCKS 65536

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
#define UU_SYS_A \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define UU_SYS_B \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define UU_STATE \
    (const unsigned char *)"\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"

static const struct gpt_part_table tbl_default[] = {
    { .uu = UU_SYS_A, .description = "System A", .size = SZ_KiB(512) },
    { .uu = UU_SYS_B, .description = "System B", .size = SZ_KiB(512) },
    { .uu = UU_STATE, .description = "State", .size = 512 },
};

/* The disk minus 2 * 34 blocks for the primary and backup GPT */
static const struct gpt_part_table tbl_largest[] = {
    { .uu = UU_SYS_A, .description = "Large", .size = SZ_KiB(32734) },
};

static const struct gpt_part_table tbl_too_large[] = {
    { .uu = UU_SYS_A, .description = "Too large", .size = SZ_KiB(32735) },
};

/* GPT partition names are at most 36 characters */
static const struct gpt_part_table tbl_long_name[] = {
    { .uu = UU_SYS_A, .description = "A name that is longer than 36 characters", .size = 512 },
};

static const struct gpt_table_list tables[] = {
    { .name = "Default", .variant = 0, .table = tbl_default, .table_length = 3 },
    { .name = "Largest", .variant = 1, .table = tbl_largest, .table_length = 1 },
    { .name = "Too large", .variant = 2, .table = tbl_too_large, .table_length = 1 },
    { .name = "Long name", .variant = 3, .table = tbl_long_name, .table_length = 1 },
};

struct fixture {
    struct ramdisk *disk;
    bio_dev_t dev;
};

/* Simulate a boot: fresh block device pool, same disk contents */
static int boot(struct fixture *f)
{
    bio_reset();
    f->dev = ramdisk_attach(f->disk, UU_DISK);
    assert_true(f->dev >= 0);
    return gpt_ptbl_init(f->dev, tables, ARRAY_SIZE(tables));
}

static int setup(void **state)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
    f->disk = ramdisk_new(DISK_BLOCKS);
    assert_non_null(f->disk);
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

static void assert_part(const unsigned char *uu, int first, int last, const char *description)
{
    bio_dev_t part = bio_get_part_by_uu(uu);

    assert_true(part >= 0);
    assert_int_equal(bio_get_first_block(part), first);
    assert_int_equal(bio_get_last_block(part), last);
    assert_string_equal(bio_get_description(part), description);
}

static void install_and_reboot(struct fixture *f, int variant)
{
    (void)boot(f);
    assert_int_equal(bio_install_partition_table(UU_DISK, variant), PB_OK);
    assert_int_equal(boot(f), PB_OK);
}

static void test_blank_disk_has_no_partitions(void **state)
{
    struct fixture *f = *state;

    assert_int_not_equal(boot(f), PB_OK);
    assert_true(bio_get_part_by_uu(UU_SYS_A) < 0);
}

static void test_install_default_table(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);

    /* Partitions start after the protective MBR, GPT header and 32 blocks
     * of partition entries */
    assert_part(UU_SYS_A, 34, 1057, "System A");
    assert_part(UU_SYS_B, 1058, 2081, "System B");
    assert_part(UU_STATE, 2082, 2082, "State");
}

static void test_install_largest_partition(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 1);
    assert_part(UU_SYS_A, 34, 34 + SZ_KiB(32734) / 512 - 1, "Large");
}

static void test_install_too_large_partition_fails(void **state)
{
    struct fixture *f = *state;

    (void)boot(f);
    assert_int_not_equal(bio_install_partition_table(UU_DISK, 2), PB_OK);
}

static void test_install_unknown_variant_fails(void **state)
{
    struct fixture *f = *state;

    (void)boot(f);
    assert_int_not_equal(bio_install_partition_table(UU_DISK, 42), PB_OK);
}

static void test_recover_from_backup(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);

    /* Destroy the primary GPT header */
    memset(ramdisk_block(f->disk, 1), 0, RAMDISK_BLOCK_SIZE);

    assert_int_equal(boot(f), PB_OK);
    assert_part(UU_SYS_A, 34, 1057, "System A");

    /* The primary header has been restored */
    assert_memory_equal(ramdisk_block(f->disk, 1), "EFI PART", 8);
    assert_int_equal(boot(f), PB_OK);
}

static void test_both_tables_corrupt_fails(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);

    memset(ramdisk_block(f->disk, 1), 0, RAMDISK_BLOCK_SIZE);
    memset(ramdisk_block(f->disk, DISK_BLOCKS - 1), 0, RAMDISK_BLOCK_SIZE);

    assert_int_not_equal(boot(f), PB_OK);
    assert_true(bio_get_part_by_uu(UU_SYS_A) < 0);
}

/* Offsets in the on-disk GPT header */
#define HDR_CRC           16
#define HDR_NO_OF_PARTS   80
#define HDR_PART_ENTRY_SZ 84
#define HDR_SIZE          92

/* Change a header field and update the header CRC, so that only the field is wrong */
static void set_hdr_field(uint8_t *hdr, size_t offset, uint32_t value)
{
    uint32_t crc = 0;

    memcpy(&hdr[offset], &value, sizeof(value));
    memcpy(&hdr[HDR_CRC], &crc, sizeof(crc));
    crc = crc32(~0L, hdr, HDR_SIZE) ^ ~0UL;
    memcpy(&hdr[HDR_CRC], &crc, sizeof(crc));
}

static uint32_t get_hdr_field(const uint8_t *hdr, size_t offset)
{
    uint32_t value;
    memcpy(&value, &hdr[offset], sizeof(value));
    return value;
}

/* F-056: more than 128 entries would make the CRC check read, and the recovery
 * write, past the in-memory table. Caught by ASan. */
static void test_primary_with_too_many_entries_recovers(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);
    set_hdr_field(ramdisk_block(f->disk, 1), HDR_NO_OF_PARTS, 129);

    assert_int_equal(boot(f), PB_OK);
    assert_part(UU_SYS_A, 34, 1057, "System A");
    assert_int_equal(get_hdr_field(ramdisk_block(f->disk, 1), HDR_NO_OF_PARTS), 128);
}

static void test_backup_with_too_many_entries_is_not_used(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);
    memset(ramdisk_block(f->disk, 1), 0, RAMDISK_BLOCK_SIZE);
    set_hdr_field(ramdisk_block(f->disk, DISK_BLOCKS - 1), HDR_NO_OF_PARTS, 132);

    assert_int_not_equal(boot(f), PB_OK);
}

static void test_unsupported_entry_size_recovers(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);
    set_hdr_field(ramdisk_block(f->disk, 1), HDR_PART_ENTRY_SZ, 256);

    assert_int_equal(boot(f), PB_OK);
    assert_int_equal(get_hdr_field(ramdisk_block(f->disk, 1), HDR_PART_ENTRY_SZ), 128);
}

static void test_install_long_name_fails(void **state)
{
    struct fixture *f = *state;

    (void)boot(f);
    assert_int_equal(bio_install_partition_table((unsigned char *)UU_DISK, 3), -PB_ERR);
    assert_int_equal(bio_install_partition_table((unsigned char *)UU_DISK, -1), -PB_ERR_PARAM);
}

static void test_init_invalid_device(void **state)
{
    (void)state;

    bio_reset();
    assert_int_equal(gpt_ptbl_init(-1, tables, ARRAY_SIZE(tables)), -PB_ERR_PARAM);
}

static void test_primary_header_crc_error_recovers(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);
    /* A field changes, but the CRC isn't updated */
    ramdisk_block(f->disk, 1)[HDR_NO_OF_PARTS] ^= 1;

    assert_int_equal(boot(f), PB_OK);
    assert_int_equal(get_hdr_field(ramdisk_block(f->disk, 1), HDR_NO_OF_PARTS), 128);
}

static void test_backup_part_array_corrupt_is_not_used(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);
    memset(ramdisk_block(f->disk, 1), 0, RAMDISK_BLOCK_SIZE);
    /* The first entry in the backup partition array */
    ramdisk_block(f->disk, DISK_BLOCKS - 33)[0] ^= 1;

    assert_int_equal(boot(f), -PB_ERR);
}

static void test_install_write_errors(void **state)
{
    struct fixture *f = *state;

    /* Protective MBR, primary table, backup table */
    for (int writes = 0; writes < 3; writes++) {
        (void)boot(f);
        f->disk->fail_writes_after = writes;
        assert_int_equal(bio_install_partition_table((unsigned char *)UU_DISK, 0), -PB_ERR_IO);
        f->disk->fail_writes_after = -1;
    }
}

static void test_init_read_errors(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);

    /* Primary, then backup */
    for (int reads = 0; reads < 2; reads++) {
        f->disk->fail_reads_after = reads;
        assert_int_equal(boot(f), -PB_ERR_IO);
    }
}

static void test_recovery_write_errors(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);
    memset(ramdisk_block(f->disk, 1), 0, RAMDISK_BLOCK_SIZE);
    f->disk->fail_writes_after = 0;
    assert_int_equal(boot(f), -PB_ERR);

    f->disk->fail_writes_after = -1;
    assert_int_equal(boot(f), PB_OK);
    memset(ramdisk_block(f->disk, DISK_BLOCKS - 1), 0, RAMDISK_BLOCK_SIZE);
    f->disk->fail_writes_after = 0;
    assert_int_equal(boot(f), -PB_ERR);
}

static void test_too_many_block_devices(void **state)
{
    struct fixture *f = *state;

    install_and_reboot(f, 0);

    /* Leave room for the disk and two of the three partitions */
    bio_reset();
    f->dev = ramdisk_attach(f->disk, UU_DISK);
    assert_true(f->dev >= 0);
    for (int i = 0; i < CONFIG_BIO_MAX_DEVS - 3; i++)
        assert_true(bio_allocate(0, 1, RAMDISK_BLOCK_SIZE, UU_SYS_B, "Filler") >= 0);

    assert_int_equal(gpt_ptbl_init(f->dev, tables, ARRAY_SIZE(tables)), -PB_ERR_MEM);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_blank_disk_has_no_partitions, setup, teardown),
        cmocka_unit_test_setup_teardown(test_install_default_table, setup, teardown),
        cmocka_unit_test_setup_teardown(test_install_largest_partition, setup, teardown),
        cmocka_unit_test_setup_teardown(test_install_too_large_partition_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_install_unknown_variant_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_recover_from_backup, setup, teardown),
        cmocka_unit_test_setup_teardown(test_both_tables_corrupt_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_primary_with_too_many_entries_recovers, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_backup_with_too_many_entries_is_not_used, setup, teardown),
        cmocka_unit_test_setup_teardown(test_unsupported_entry_size_recovers, setup, teardown),
        cmocka_unit_test_setup_teardown(test_install_long_name_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_init_invalid_device, setup, teardown),
        cmocka_unit_test_setup_teardown(test_primary_header_crc_error_recovers, setup, teardown),
        cmocka_unit_test_setup_teardown(test_backup_part_array_corrupt_is_not_used, setup, teardown),
        cmocka_unit_test_setup_teardown(test_install_write_errors, setup, teardown),
        cmocka_unit_test_setup_teardown(test_init_read_errors, setup, teardown),
        cmocka_unit_test_setup_teardown(test_recovery_write_errors, setup, teardown),
        cmocka_unit_test_setup_teardown(test_too_many_block_devices, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
