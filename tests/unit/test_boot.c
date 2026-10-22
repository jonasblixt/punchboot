/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The boot flow (src/boot/boot.c) with fake image helpers and a fake boot
 * driver, for the boot sources and error paths the integration tests can't
 * reach. The image helpers themselves are tested in test_image_helpers.c.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/boot.h>
#include <boot/image_helpers.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/ramdisk.h"

#define DISK_BLOCKS 64

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
#define UU_SYS_A \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define UU_SYS_B \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define UU_OTHER \
    (const unsigned char *)"\xff\x4d\xdc\x6c\xad\x7a\x47\xe8\x87\x73\x67\x29\x39\x2d\xd1\xb5"

/* Return codes of the fakes, and what they saw */
static struct {
    int auth_rc;
    int verify_parts_rc;
    int load_rc;
    int verify_payload_rc;
    int load_calls;
    boot_read_cb_t load_read_f;
    boot_result_cb_t load_result_f;

    int early_rc;
    bio_dev_t boot_dev;
    int prepare_rc;
    int prepare_calls;
    uuid_t prepare_uu;
    struct bpak_header *prepare_hdr;
    int late_rc;
    int late_calls;
    uint32_t late_flags;
    int jump_calls;
    int set_part_calls;
    int in_mem_rc;
    int custom_rc;
    int custom_calls;

    int read_cb_rc;
    int read_cb_calls;
    int read_cb_offset;
    int result_cb_calls;
    int last_result;
} fake;

static struct bpak_header in_mem_header;

int boot_image_auth_header(struct bpak_header *hdr)
{
    (void)hdr;
    return fake.auth_rc;
}

int boot_image_verify_parts(struct bpak_header *hdr)
{
    (void)hdr;
    return fake.verify_parts_rc;
}

int boot_image_load_and_hash(struct bpak_header *hdr,
                             size_t load_chunk_size,
                             boot_read_cb_t read_f,
                             boot_result_cb_t result_f,
                             uint8_t *payload_digest,
                             size_t payload_digest_size)
{
    (void)hdr;
    (void)load_chunk_size;
    (void)payload_digest;
    (void)payload_digest_size;
    fake.load_calls++;
    fake.load_read_f = read_f;
    fake.load_result_f = result_f;
    return fake.load_rc;
}

int boot_image_verify_payload(struct bpak_header *hdr, uint8_t *payload_digest)
{
    (void)hdr;
    (void)payload_digest;
    return fake.verify_payload_rc;
}

static int fake_early_boot(void)
{
    return fake.early_rc;
}

static bio_dev_t fake_get_boot_bio_device(void)
{
    return fake.boot_dev;
}

static int fake_set_boot_partition(uuid_t part_uu)
{
    (void)part_uu;
    fake.set_part_calls++;
    return PB_OK;
}

static void fake_get_boot_partition(uuid_t part_uu)
{
    uuid_copy(part_uu, UU_SYS_B);
}

static int fake_get_in_mem_image(struct bpak_header **header)
{
    *header = &in_mem_header;
    return fake.in_mem_rc;
}

static int fake_authenticate_image(struct bpak_header **header_ptr)
{
    (void)header_ptr;
    fake.custom_calls++;
    return fake.custom_rc;
}

static int fake_prepare(struct bpak_header *header, uuid_t boot_part_uu)
{
    fake.prepare_calls++;
    fake.prepare_hdr = header;
    uuid_copy(fake.prepare_uu, boot_part_uu);
    return fake.prepare_rc;
}

static int fake_late_boot(struct bpak_header *header, uuid_t boot_part_uu)
{
    (void)header;
    (void)boot_part_uu;
    fake.late_calls++;
    fake.late_flags = boot_get_flags();
    return fake.late_rc;
}

static void fake_jump(void)
{
    fake.jump_calls++;
}

static int fake_read_cb(int block_offset, size_t length, void *buf)
{
    fake.read_cb_calls++;
    fake.read_cb_offset = block_offset;
    memset(buf, 0, length);
    return fake.read_cb_rc;
}

static int fake_result_cb(int result)
{
    fake.result_cb_calls++;
    fake.last_result = result;
    return PB_OK;
}

static const struct boot_driver driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .early_boot_cb = fake_early_boot,
    .get_boot_bio_device = fake_get_boot_bio_device,
    .set_boot_partition = fake_set_boot_partition,
    .get_boot_partition = fake_get_boot_partition,
    .get_in_mem_image = fake_get_in_mem_image,
    .authenticate_image = fake_authenticate_image,
    .prepare = fake_prepare,
    .late_boot_cb = fake_late_boot,
    .jump = fake_jump,
};

/* Only what boot_init() requires */
static const struct boot_driver minimal_driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .jump = fake_jump,
};

struct fixture {
    struct ramdisk *disk;
    bio_dev_t sys_a;
    bio_dev_t sys_b;
};

static int setup(void **state)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
    f->disk = ramdisk_new(DISK_BLOCKS);
    assert_non_null(f->disk);

    bio_reset();
    bio_dev_t disk = ramdisk_attach(f->disk, UU_DISK);
    assert_true(disk >= 0);
    /* The bpak header (4 KiB) is read from the end of the partition */
    f->sys_a = bio_allocate_parent(disk, 10, 29, RAMDISK_BLOCK_SIZE, UU_SYS_A, "System A");
    f->sys_b = bio_allocate_parent(disk, 30, 49, RAMDISK_BLOCK_SIZE, UU_SYS_B, "System B");
    assert_true(f->sys_a >= 0 && f->sys_b >= 0);
    assert_int_equal(bio_clear_set_flags(f->sys_a, 0, BIO_FLAG_BOOTABLE), PB_OK);

    memset(&fake, 0, sizeof(fake));
    fake.boot_dev = f->sys_a;
    assert_int_equal(boot_init(&driver), PB_OK);
    boot_configure_load_cb(NULL, NULL);

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

/* Runs first: no boot driver yet */
static void test_not_initialized(void **state)
{
    uuid_t uu;
    (void)state;

    assert_int_equal(boot_load(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_set_boot_partition((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);
    memset(uu, 0xff, sizeof(uu));
    boot_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));
}

static void test_init_rejects(void **state)
{
    static const struct boot_driver no_jump = { .default_boot_source = BOOT_SOURCE_BIO };
    (void)state;

    assert_int_equal(boot_init(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_init(&no_jump), -PB_ERR_PARAM);
}

static void test_set_source(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_INVALID), -PB_ERR_PARAM);
    assert_int_equal(boot_set_source(BOOT_SOURCE_END), -PB_ERR_PARAM);
    assert_int_equal(boot_set_source(BOOT_SOURCE_CUSTOM), PB_OK);
}

static void test_boot_partition(void **state)
{
    uuid_t uu;
    (void)state;

    assert_int_equal(boot_set_boot_partition(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_set_boot_partition((unsigned char *)UU_SYS_A), PB_OK);
    assert_int_equal(fake.set_part_calls, 1);

    boot_get_boot_partition(NULL);
    boot_get_boot_partition(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);

    assert_int_equal(boot_init(&minimal_driver), PB_OK);
    assert_int_equal(boot_set_boot_partition((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);
    memset(uu, 0xff, sizeof(uu));
    boot_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));
}

static void test_flags(void **state)
{
    (void)state;

    boot_clear_set_flags(0, BOOT_FLAG_CMD | BOOT_FLAG_VERBOSE);
    assert_int_equal(boot_get_flags(), BOOT_FLAG_CMD | BOOT_FLAG_VERBOSE);
    boot_clear_set_flags(BOOT_FLAG_CMD, 0);
    assert_int_equal(boot_get_flags(), BOOT_FLAG_VERBOSE);
}

static void test_bio_boot(void **state)
{
    struct fixture *f = *state;

    boot_clear_set_flags(0, BOOT_FLAG_VERBOSE);
    assert_int_equal(boot(NULL), -PB_ERR);

    assert_int_equal(fake.load_calls, 1);
    assert_non_null(fake.load_read_f);
    assert_null(fake.load_result_f);
    assert_int_equal(fake.prepare_calls, 1);
    assert_memory_equal(fake.prepare_uu, UU_SYS_A, 16);
    assert_int_equal(fake.late_calls, 1);
    /* late_boot sees the flags (F-060) */
    assert_int_equal(fake.late_flags, BOOT_FLAG_VERBOSE);
    /* The fake jump returns, which boot_jump() reports as an error */
    assert_int_equal(fake.jump_calls, 1);
    /* A failed boot clears the flags */
    assert_int_equal(boot_get_flags(), 0);
    (void)f;
}

static void test_bio_boot_override(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);
    assert_memory_equal(fake.prepare_uu, UU_SYS_B, 16);

    assert_int_equal(boot_load((unsigned char *)UU_OTHER), -PB_ERR_NOT_FOUND);
}

static void test_bio_boot_errors(void **state)
{
    struct fixture *f = *state;

    /* B is not bootable */
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), -PB_ERR_PART_NOT_BOOTABLE);

    fake.boot_dev = -PB_ERR_NO_ACTIVE_BOOT_PARTITION;
    assert_int_equal(boot_load(NULL), -PB_ERR_NO_ACTIVE_BOOT_PARTITION);
    fake.boot_dev = -PB_ERR;
    assert_int_equal(boot_load(NULL), -PB_ERR);
    fake.boot_dev = f->sys_a;

    f->disk->fail_reads_after = 0;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    f->disk->fail_reads_after = -1;

    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;
    assert_int_equal(boot_load(NULL), -PB_ERR_AUTHENTICATION_FAILED);
    fake.auth_rc = PB_OK;

    fake.verify_parts_rc = -PB_ERR_MEM;
    assert_int_equal(boot_load(NULL), -PB_ERR_MEM);
    fake.verify_parts_rc = PB_OK;

    fake.load_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    fake.load_rc = PB_OK;

    fake.verify_payload_rc = -PB_ERR_BAD_PAYLOAD;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_PAYLOAD);
    fake.verify_payload_rc = PB_OK;

    fake.prepare_rc = -PB_ERR_BAD_META;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_META);
    fake.prepare_rc = PB_OK;

    assert_int_equal(fake.late_calls, 0);
    assert_int_equal(fake.jump_calls, 0);
}

static void test_bio_boot_needs_a_device_callback(void **state)
{
    (void)state;

    assert_int_equal(boot_init(&minimal_driver), PB_OK);
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
}

static void test_early_boot_abort(void **state)
{
    (void)state;

    fake.early_rc = -PB_ERR_ABORT;
    boot_clear_set_flags(0, BOOT_FLAG_CMD);
    assert_int_equal(boot(NULL), -PB_ERR_ABORT);
    assert_int_equal(fake.load_calls, 0);
    assert_int_equal(boot_get_flags(), 0);
}

static void test_late_boot_error(void **state)
{
    (void)state;

    fake.late_rc = -PB_ERR_IO;
    boot_clear_set_flags(0, BOOT_FLAG_CMD);
    assert_int_equal(boot(NULL), -PB_ERR_IO);
    assert_int_equal(fake.late_flags, BOOT_FLAG_CMD);
    assert_int_equal(fake.jump_calls, 0);
    assert_int_equal(boot_get_flags(), 0);
}

static void test_in_mem_boot(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_IN_MEM), PB_OK);
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_ptr_equal(fake.prepare_hdr, &in_mem_header);
    /* Already in memory: nothing to read */
    assert_null(fake.load_read_f);
}

static void test_in_mem_boot_errors(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_IN_MEM), PB_OK);

    fake.in_mem_rc = -PB_ERR_NOT_FOUND;
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_FOUND);
    fake.in_mem_rc = PB_OK;

    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;
    assert_int_equal(boot_load(NULL), -PB_ERR_AUTHENTICATION_FAILED);
    fake.auth_rc = PB_OK;

    fake.verify_parts_rc = -PB_ERR_MEM;
    assert_int_equal(boot_load(NULL), -PB_ERR_MEM);
    fake.verify_parts_rc = PB_OK;

    fake.load_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    fake.load_rc = PB_OK;

    fake.verify_payload_rc = -PB_ERR_BAD_PAYLOAD;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_PAYLOAD);
    fake.verify_payload_rc = PB_OK;

    assert_int_equal(fake.prepare_calls, 0);

    assert_int_equal(boot_init(&minimal_driver), PB_OK);
    assert_int_equal(boot_set_source(BOOT_SOURCE_IN_MEM), PB_OK);
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
}

static void test_cb_boot(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);

    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);
    /* The header comes first, at a negative offset */
    assert_int_equal(fake.read_cb_calls, 1);
    assert_int_equal(fake.read_cb_offset, -(int)(sizeof(struct bpak_header) / 512));
    assert_ptr_equal(fake.load_read_f, fake_read_cb);
    assert_ptr_equal(fake.load_result_f, fake_result_cb);
    /* After the header check and after the payload check */
    assert_int_equal(fake.result_cb_calls, 2);
    assert_int_equal(fake.last_result, PB_OK);
    assert_memory_equal(fake.prepare_uu, UU_SYS_B, 16);
}

static void test_cb_boot_errors(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);

    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);

    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    fake.read_cb_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    assert_int_equal(fake.result_cb_calls, 0);
    fake.read_cb_rc = PB_OK;

    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;
    assert_int_equal(boot_load(NULL), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(fake.result_cb_calls, 1);
    assert_int_equal(fake.last_result, -PB_ERR_AUTHENTICATION_FAILED);
    fake.auth_rc = PB_OK;

    fake.verify_parts_rc = -PB_ERR_MEM;
    assert_int_equal(boot_load(NULL), -PB_ERR_MEM);
    assert_int_equal(fake.last_result, -PB_ERR_MEM);
    fake.verify_parts_rc = PB_OK;

    /* The host still gets the final result (F-061) */
    fake.result_cb_calls = 0;
    fake.load_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    assert_int_equal(fake.result_cb_calls, 2);
    assert_int_equal(fake.last_result, -PB_ERR_IO);
    fake.load_rc = PB_OK;

    fake.verify_payload_rc = -PB_ERR_BAD_PAYLOAD;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_PAYLOAD);
    assert_int_equal(fake.last_result, -PB_ERR_BAD_PAYLOAD);
    fake.verify_payload_rc = PB_OK;

    assert_int_equal(fake.prepare_calls, 0);

    /* The result callback is optional */
    boot_configure_load_cb(fake_read_cb, NULL);
    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;
    assert_int_equal(boot_load(NULL), -PB_ERR_AUTHENTICATION_FAILED);
    fake.auth_rc = PB_OK;
    assert_int_equal(boot_load(NULL), PB_OK);
}

static void test_custom_boot(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_CUSTOM), PB_OK);
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.custom_calls, 1);

    fake.custom_rc = -PB_ERR_SIGNATURE;
    assert_int_equal(boot_load(NULL), -PB_ERR_SIGNATURE);

    assert_int_equal(boot_init(&minimal_driver), PB_OK);
    assert_int_equal(boot_set_source(BOOT_SOURCE_CUSTOM), PB_OK);
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
}

static void test_invalid_default_source(void **state)
{
    static const struct boot_driver bad_source = {
        .default_boot_source = BOOT_SOURCE_INVALID,
        .jump = fake_jump,
    };
    (void)state;

    assert_int_equal(boot_init(&bad_source), PB_OK);
    assert_int_equal(boot_load(NULL), -PB_ERR_PARAM);
}

static void test_jump_removed_after_init(void **state)
{
    static struct boot_driver mutable_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .jump = fake_jump,
    };
    (void)state;

    assert_int_equal(boot_init(&mutable_driver), PB_OK);
    mutable_driver.jump = NULL;
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
    mutable_driver.jump = fake_jump;
}

#define BOOT_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_not_initialized),
        BOOT_TEST(test_init_rejects),
        BOOT_TEST(test_set_source),
        BOOT_TEST(test_boot_partition),
        BOOT_TEST(test_flags),
        BOOT_TEST(test_bio_boot),
        BOOT_TEST(test_bio_boot_override),
        BOOT_TEST(test_bio_boot_errors),
        BOOT_TEST(test_bio_boot_needs_a_device_callback),
        BOOT_TEST(test_early_boot_abort),
        BOOT_TEST(test_late_boot_error),
        BOOT_TEST(test_in_mem_boot),
        BOOT_TEST(test_in_mem_boot_errors),
        BOOT_TEST(test_cb_boot),
        BOOT_TEST(test_cb_boot_errors),
        BOOT_TEST(test_custom_boot),
        BOOT_TEST(test_invalid_default_source),
        BOOT_TEST(test_jump_removed_after_init),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
