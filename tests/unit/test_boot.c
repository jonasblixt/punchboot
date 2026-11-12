/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The boot flow (src/boot/boot.c) with fake image helpers and a fake boot
 * driver, for the boot sources and error paths the integration tests can't
 * reach. The image helpers themselves are tested in test_image_helpers.c,
 * the static partition table in test_static_ptbl.c.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <boot/boot.h>
#include <boot/image_helpers.h>
#include <drivers/partition/static.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/plat.h"
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
#define UU_TINY \
    (const unsigned char *)"\x5b\x0e\x3a\x91\x6c\x2f\x4d\x18\xb7\x44\x0a\x9d\x53\xe1\x7c\x26"

/* System A and B, as the fixtures lay them out */
#define SYS_A_FIRST_BLOCK 10
#define SYS_BLOCKS        20
#define SYS_SIZE          (SYS_BLOCKS * RAMDISK_BLOCK_SIZE)

/*
 * The ramdisk as a memory mapped device: A is booted in place, B is loaded,
 * and Tiny has no room for a header.
 */
static const struct static_part_table mapped_table[] = {
    {
        .uu = UU_SYS_A,
        .description = "System A",
        .first_lba = SYS_A_FIRST_BLOCK,
        .size = SYS_SIZE,
        .flags = STATIC_PART_FLAG_IN_PLACE,
    },
    {
        .uu = UU_SYS_B,
        .description = "System B",
        .size = SYS_SIZE,
    },
    {
        .uu = UU_TINY,
        .description = "Tiny",
        .size = RAMDISK_BLOCK_SIZE,
        .flags = STATIC_PART_FLAG_IN_PLACE,
    },
    {
        .uu = NULL,
    },
};

static const struct static_part_table nothing_mapped[] = {
    {
        .uu = NULL,
    },
};

/* Return codes of the fakes, and what they saw */
static struct {
    int auth_rc;
    int auth_calls;
    struct bpak_header *auth_hdr;
    int verify_parts_rc;
    int verify_parts_calls;
    int invalidate_calls;
    uintptr_t invalidate_start;
    size_t invalidate_len;
    /* How many headers were authenticated when the cache was invalidated */
    int invalidate_after_auth_calls;
    int in_place_rc;
    int in_place_calls;
    struct bpak_header *in_place_hdr;
    uintptr_t in_place_base;
    size_t in_place_size;
    int load_rc;
    int verify_payload_rc;
    int verify_payload_calls;
    int load_calls;
    struct bpak_header *load_hdr;
    boot_read_cb_t load_read_f;
    boot_result_cb_t load_result_f;

    int early_rc;

    /* The fake policy: it hands out candidates[], then -PB_ERR_NOT_FOUND */
    const unsigned char *candidates[3];
    size_t no_of_candidates;
    size_t next_candidate;
    int next_rc;
    /* This call of next fails with -PB_ERR_IO, 0: none */
    int next_fail_call;
    int next_calls;
    int report_calls;
    uuid_t report_uu[3];
    int report_result[3];
    int activate_calls;

    int prepare_rc;
    int prepare_calls;
    uuid_t prepare_uu;
    struct bpak_header *prepare_hdr;
    int late_rc;
    int late_calls;
    uint32_t late_flags;
    int jump_calls;
    /* prepare fails for this partition */
    const unsigned char *prepare_fail_uu;

    int read_cb_rc;
    int read_cb_calls;
    int64_t read_cb_offset;
    int result_cb_calls;
    int last_result;
} fake;

/* The header as boot_image_auth_header() got it */
static struct bpak_header auth_seen;

/* test_boot links no arch. The only cache call of the flow is the one that
 * drops what the cache holds of a partition that is verified in place */
void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    fake.invalidate_calls++;
    fake.invalidate_start = start;
    fake.invalidate_len = len;
    fake.invalidate_after_auth_calls = fake.auth_calls;
}

int boot_image_auth_header(struct bpak_header *hdr)
{
    fake.auth_calls++;
    fake.auth_hdr = hdr;
    memcpy(&auth_seen, hdr, sizeof(auth_seen));
    /* As bpak_copyz_signature() does: the header must be pb's own copy */
    memset(hdr->signature, 0, sizeof(hdr->signature));
    hdr->signature_sz = 0;
    return fake.auth_rc;
}

int boot_image_verify_parts(struct bpak_header *hdr)
{
    (void)hdr;
    fake.verify_parts_calls++;
    return fake.verify_parts_rc;
}

int boot_image_verify_parts_in_place(struct bpak_header *hdr, uintptr_t base, size_t size)
{
    fake.in_place_calls++;
    fake.in_place_hdr = hdr;
    fake.in_place_base = base;
    fake.in_place_size = size;
    return fake.in_place_rc;
}

int boot_image_load_and_hash(struct bpak_header *hdr,
                             size_t load_chunk_size,
                             boot_read_cb_t read_f,
                             boot_result_cb_t result_f,
                             uint8_t *payload_digest,
                             size_t payload_digest_size)
{
    (void)load_chunk_size;
    (void)payload_digest;
    (void)payload_digest_size;
    fake.load_calls++;
    fake.load_hdr = hdr;
    fake.load_read_f = read_f;
    fake.load_result_f = result_f;
    return fake.load_rc;
}

int boot_image_verify_payload(struct bpak_header *hdr, uint8_t *payload_digest)
{
    (void)hdr;
    (void)payload_digest;
    fake.verify_payload_calls++;
    return fake.verify_payload_rc;
}

static int fake_early_boot(void)
{
    return fake.early_rc;
}

static int fake_policy_init(const struct boot_policy *policy)
{
    (void)policy;
    return PB_OK;
}

static int fake_policy_next(uuid_t part_uu)
{
    fake.next_calls++;

    if (fake.next_rc != PB_OK)
        return fake.next_rc;

    if (fake.next_calls == fake.next_fail_call) {
        fake.next_candidate = 0;
        return -PB_ERR_IO;
    }

    if (fake.next_candidate >= fake.no_of_candidates) {
        fake.next_candidate = 0;
        return -PB_ERR_NOT_FOUND;
    }

    uuid_copy(part_uu, fake.candidates[fake.next_candidate++]);
    return PB_OK;
}

static void fake_policy_report(const uuid_t part_uu, int result)
{
    /* The first three are kept */
    if (fake.report_calls < 3) {
        uuid_copy(fake.report_uu[fake.report_calls], part_uu);
        fake.report_result[fake.report_calls] = result;
    }

    fake.report_calls++;

    if (result == PB_OK)
        fake.next_candidate = 0;
}

static enum boot_policy_mode fake_policy_mode(void)
{
    return BOOT_POLICY_MODE_NORMAL;
}

static int fake_policy_activate(uuid_t part_uu)
{
    (void)part_uu;
    fake.activate_calls++;
    return PB_OK;
}

static void fake_policy_status(uuid_t part_uu)
{
    uuid_copy(part_uu, UU_SYS_B);
}

static const struct boot_policy_ops fake_policy_ops = {
    .init = fake_policy_init,
    .next = fake_policy_next,
    .report = fake_policy_report,
    .mode = fake_policy_mode,
    .activate = fake_policy_activate,
    .status = fake_policy_status,
};

static const struct boot_policy fake_policy = { .ops = &fake_policy_ops };

static int fake_prepare(struct bpak_header *header, uuid_t boot_part_uu)
{
    fake.prepare_calls++;
    fake.prepare_hdr = header;
    uuid_copy(fake.prepare_uu, boot_part_uu);

    if (fake.prepare_fail_uu != NULL && uuid_compare(boot_part_uu, fake.prepare_fail_uu) == 0)
        return -PB_ERR_BAD_META;

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

static int fake_read_cb(int64_t offset, size_t length, void *buf)
{
    fake.read_cb_calls++;
    fake.read_cb_offset = offset;
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
    .policy = &fake_policy,
    .prepare = fake_prepare,
    .late_boot_cb = fake_late_boot,
    .jump = fake_jump,
};

/* Only what boot_init() requires: no policy */
static const struct boot_driver minimal_driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .jump = fake_jump,
};

/* A board without a policy has the object, without ops */
static const struct boot_policy no_policy;

static const struct boot_driver no_policy_driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .policy = &no_policy,
    .prepare = fake_prepare,
    .jump = fake_jump,
};

struct fixture {
    struct ramdisk *disk;
    bio_dev_t sys_a;
    bio_dev_t sys_b;
    bio_dev_t tiny;
};

/* Where System A (0) or B (1) is in the ramdisk's memory */
static uint8_t *sys_start(struct fixture *f, int system)
{
    return ramdisk_block(f->disk, SYS_A_FIRST_BLOCK + system * SYS_BLOCKS);
}

/* The 4 KiB at its end, where a partition has the header */
static uint8_t *sys_header(struct fixture *f, int system)
{
    return sys_start(f, system) + SYS_SIZE - sizeof(struct bpak_header);
}

static struct fixture *new_fixture(bio_dev_t *disk)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
    f->disk = ramdisk_new(DISK_BLOCKS);
    assert_non_null(f->disk);

    bio_reset();
    *disk = ramdisk_attach(f->disk, UU_DISK);
    assert_true(*disk >= 0);
    return f;
}

static void reset_fakes(struct fixture *f)
{
    memset(&fake, 0, sizeof(fake));
    memset(&auth_seen, 0, sizeof(auth_seen));
    (void)f;
    /* The policy's one candidate is System A */
    fake.candidates[0] = UU_SYS_A;
    fake.no_of_candidates = 1;
    fake_lockdown_calls = 0;
    fake_lockdown_rc = PB_OK;
    assert_int_equal(boot_init(&driver), PB_OK);
    boot_configure_load_cb(NULL, NULL);
}

/* Nothing is mapped: every partition is loaded */
static int setup(void **state)
{
    bio_dev_t disk;
    struct fixture *f = new_fixture(&disk);

    /* The static table's driver remembers the mapped table of the case
     * before, whose memory is gone. One without partitions replaces it: the
     * driver takes another table once bio_reset() took the partitions that
     * were booted in place */
    assert_int_equal(static_ptbl_init_mapped(disk, 0, nothing_mapped), PB_OK);

    /* The bpak header (4 KiB) is read from the end of the partition */
    f->sys_a = bio_allocate_parent(disk,
                                   SYS_A_FIRST_BLOCK,
                                   SYS_A_FIRST_BLOCK + SYS_BLOCKS - 1,
                                   RAMDISK_BLOCK_SIZE,
                                   UU_SYS_A,
                                   "System A");
    f->sys_b = bio_allocate_parent(disk,
                                   SYS_A_FIRST_BLOCK + SYS_BLOCKS,
                                   SYS_A_FIRST_BLOCK + 2 * SYS_BLOCKS - 1,
                                   RAMDISK_BLOCK_SIZE,
                                   UU_SYS_B,
                                   "System B");
    f->tiny = -1;
    assert_true(f->sys_a >= 0 && f->sys_b >= 0);
    assert_int_equal(bio_clear_set_flags(f->sys_a, 0, BIO_FLAG_BOOTABLE), PB_OK);

    reset_fakes(f);
    *state = f;
    return 0;
}

/* The same layout from mapped_table, the ramdisk's memory is the map */
static int setup_mapped(void **state)
{
    bio_dev_t disk;
    struct fixture *f = new_fixture(&disk);

    assert_int_equal(static_ptbl_init_mapped(disk, (uintptr_t)f->disk->data, mapped_table), PB_OK);
    f->sys_a = bio_get_part_by_uu(UU_SYS_A);
    f->sys_b = bio_get_part_by_uu(UU_SYS_B);
    f->tiny = bio_get_part_by_uu(UU_TINY);
    assert_true(f->sys_a >= 0 && f->sys_b >= 0 && f->tiny >= 0);
    assert_int_equal(bio_clear_set_flags(f->sys_a, 0, BIO_FLAG_BOOTABLE), PB_OK);

    /* What a header would be, at the end of each system */
    uint8_t *hdr_a = sys_header(f, 0);
    uint8_t *hdr_b = sys_header(f, 1);

    for (size_t i = 0; i < sizeof(struct bpak_header); i++) {
        hdr_a[i] = (uint8_t)(i * 7 + 1);
        hdr_b[i] = (uint8_t)(i * 13 + 2);
    }

    reset_fakes(f);
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
    assert_int_equal(boot_set_source(BOOT_SOURCE_BIO), PB_OK);
    /* The last one */
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
}

static void test_boot_partition(void **state)
{
    uuid_t uu;
    (void)state;

    assert_int_equal(boot_set_boot_partition(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_set_boot_partition((unsigned char *)UU_SYS_A), PB_OK);
    assert_int_equal(fake.activate_calls, 1);

    boot_get_boot_partition(NULL);
    boot_get_boot_partition(uu);
    assert_memory_equal(uu, UU_SYS_B, 16);

    /* Without a policy: nothing to activate, no partition */
    assert_int_equal(boot_init(&minimal_driver), PB_OK);
    assert_int_equal(boot_set_boot_partition((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);
    memset(uu, 0xff, sizeof(uu));
    boot_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));

    assert_int_equal(boot_init(&no_policy_driver), PB_OK);
    assert_int_equal(boot_set_boot_partition((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);
    memset(uu, 0xff, sizeof(uu));
    boot_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));
    assert_int_equal(fake.activate_calls, 1);
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
    /* Loaded: the placement check is the one for memory pb writes */
    assert_int_equal(fake.verify_parts_calls, 1);
    assert_int_equal(fake.in_place_calls, 0);
    assert_int_equal(fake.prepare_calls, 1);
    assert_memory_equal(fake.prepare_uu, UU_SYS_A, 16);
    /* The policy handed out one candidate and heard that it succeeded */
    assert_int_equal(fake.next_calls, 1);
    assert_int_equal(fake.report_calls, 1);
    assert_memory_equal(fake.report_uu[0], UU_SYS_A, 16);
    assert_int_equal(fake.report_result[0], PB_OK);
    assert_int_equal(fake.late_calls, 1);
    /* late_boot sees the flags (F-060) */
    assert_int_equal(fake.late_flags, BOOT_FLAG_VERBOSE);
    /* The platform locks what only pb may change before the jump (F-244) */
    assert_int_equal(fake_lockdown_calls, 1);
    /* The fake jump returns, which boot_jump() reports as an error */
    assert_int_equal(fake.jump_calls, 1);
    /* A partition that is loaded is read through bio, not through a map:
     * nothing of it is in the cache */
    assert_int_equal(fake.invalidate_calls, 0);
    /* A failed boot clears the flags */
    assert_int_equal(boot_get_flags(), 0);

    /* The payload is read at byte offsets, in the device's blocks (F-062) */
    uint8_t buf[RAMDISK_BLOCK_SIZE];
    memset(ramdisk_block(f->disk, 12), 0xa5, RAMDISK_BLOCK_SIZE);
    assert_int_equal(fake.load_read_f(2 * RAMDISK_BLOCK_SIZE, sizeof(buf), buf), PB_OK);
    assert_int_equal(buf[0], 0xa5);
    assert_int_equal(buf[RAMDISK_BLOCK_SIZE - 1], 0xa5);
    assert_int_equal(fake.load_read_f(100, sizeof(buf), buf), -PB_ERR_ALIGN);
    assert_int_equal(fake.load_read_f(-RAMDISK_BLOCK_SIZE, sizeof(buf), buf), -PB_ERR_ALIGN);
}

static void test_bio_boot_override(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);
    assert_memory_equal(fake.prepare_uu, UU_SYS_B, 16);

    assert_int_equal(boot_load((unsigned char *)UU_OTHER), -PB_ERR_NOT_FOUND);

    /* A partition that is named is the one candidate: the policy isn't
     * asked and hears nothing, also when it fails (boot.md 6) */
    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(fake.next_calls, 0);
    assert_int_equal(fake.report_calls, 0);
    fake.auth_rc = PB_OK;

    /* The null UUID names none: the policy's candidates, as with NULL */
    uuid_t null_uu;

    uuid_clear(null_uu);
    assert_int_equal(boot_load(null_uu), PB_OK);
    assert_memory_equal(fake.prepare_uu, UU_SYS_A, 16);
    assert_int_equal(fake.next_calls, 1);
    assert_int_equal(fake.report_calls, 1);
}

static void test_bio_boot_errors(void **state)
{
    struct fixture *f = *state;

    /* B is not bootable */
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), -PB_ERR_PART_NOT_BOOTABLE);

    /* The policy has no candidate */
    fake.no_of_candidates = 0;
    assert_int_equal(boot_load(NULL), -PB_ERR_NO_ACTIVE_BOOT_PARTITION);
    assert_int_equal(fake.report_calls, 0);
    fake.no_of_candidates = 1;

    /* Its state couldn't be written: its error */
    fake.next_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    fake.next_rc = -PB_ERR_STATE;
    assert_int_equal(boot_load(NULL), -PB_ERR_STATE);
    fake.next_rc = PB_OK;
    assert_int_equal(fake.report_calls, 0);
    assert_int_equal(fake.auth_calls, 0);

    /* A candidate that isn't in the table */
    fake.candidates[0] = UU_OTHER;
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_FOUND);
    assert_int_equal(fake.report_calls, 1);
    assert_memory_equal(fake.report_uu[0], UU_OTHER, 16);
    assert_int_equal(fake.report_result[0], -PB_ERR_NOT_FOUND);
    fake.candidates[0] = UU_SYS_A;
    fake.report_calls = 0;

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

    fake.report_calls = 0;
    fake.prepare_rc = -PB_ERR_BAD_META;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_META);
    fake.prepare_rc = PB_OK;
    /* Preparing the handover is part of the attempt: the policy hears it */
    assert_int_equal(fake.report_calls, 1);
    assert_int_equal(fake.report_result[0], -PB_ERR_BAD_META);

    assert_int_equal(fake.late_calls, 0);
    assert_int_equal(fake.jump_calls, 0);
}

/* Only a policy's candidates are bootable: without one nothing boots from
 * storage, a partition that is named neither */
static void test_bio_boot_needs_a_policy(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(boot_init(&minimal_driver), PB_OK);
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);

    assert_int_equal(boot_init(&no_policy_driver), PB_OK);
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(fake.auth_calls, 0);
    assert_int_equal(f->disk->reads, 0);

    /* The host's stream needs none */
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 1);

    /* Nor a handover that prepares nothing */
    assert_int_equal(boot_init(&minimal_driver), PB_OK);
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 1);
}

/* The first candidate fails: the policy hears it and the next one is tried
 * (boot.md 2, 3.5 rule 3, F-174) */
static void test_policy_next_candidate(void **state)
{
    struct fixture *f = *state;

    /* A isn't bootable, B is */
    assert_int_equal(bio_clear_set_flags(f->sys_a, BIO_FLAG_BOOTABLE, 0), PB_OK);
    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;

    assert_int_equal(boot(NULL), -PB_ERR);

    assert_int_equal(fake.next_calls, 2);
    assert_int_equal(fake.report_calls, 2);
    assert_memory_equal(fake.report_uu[0], UU_SYS_A, 16);
    assert_int_equal(fake.report_result[0], -PB_ERR_PART_NOT_BOOTABLE);
    assert_memory_equal(fake.report_uu[1], UU_SYS_B, 16);
    assert_int_equal(fake.report_result[1], PB_OK);
    /* B is what is prepared and started */
    assert_int_equal(fake.prepare_calls, 1);
    assert_memory_equal(fake.prepare_uu, UU_SYS_B, 16);
    assert_int_equal(fake.late_calls, 1);
    assert_int_equal(fake.jump_calls, 1);
}

/* A failed prepare is the candidate's too: the next one is tried */
static void test_policy_next_candidate_after_prepare(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;
    fake.prepare_fail_uu = UU_SYS_A;

    assert_int_equal(boot_load(NULL), PB_OK);

    assert_int_equal(fake.report_calls, 2);
    assert_int_equal(fake.report_result[0], -PB_ERR_BAD_META);
    assert_int_equal(fake.report_result[1], PB_OK);
    assert_int_equal(fake.prepare_calls, 2);
    assert_memory_equal(fake.prepare_uu, UU_SYS_B, 16);
    /* Each was loaded and verified */
    assert_int_equal(fake.auth_calls, 2);
    assert_int_equal(fake.load_calls, 2);
}

/* Every candidate failed: the first one's error, and each was reported */
static void test_policy_every_candidate_fails(void **state)
{
    struct fixture *f = *state;

    /* A fails on its signature, B isn't bootable, the third isn't there */
    (void)f;
    fake.candidates[1] = UU_SYS_B;
    fake.candidates[2] = UU_OTHER;
    fake.no_of_candidates = 3;
    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;

    assert_int_equal(boot(NULL), -PB_ERR_AUTHENTICATION_FAILED);

    /* Three candidates, then none */
    assert_int_equal(fake.next_calls, 4);
    assert_int_equal(fake.report_calls, 3);
    assert_int_equal(fake.report_result[0], -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(fake.report_result[1], -PB_ERR_PART_NOT_BOOTABLE);
    assert_int_equal(fake.report_result[2], -PB_ERR_NOT_FOUND);
    assert_int_equal(fake.prepare_calls, 0);
    assert_int_equal(fake.late_calls, 0);
    assert_int_equal(fake.jump_calls, 0);
    assert_int_equal(boot_get_flags(), 0);

    /* The next boot starts with the first candidate again */
    fake.auth_rc = PB_OK;
    fake.report_calls = 0;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_memory_equal(fake.report_uu[0], UU_SYS_A, 16);
    assert_memory_equal(fake.prepare_uu, UU_SYS_A, 16);
}

/* The policy's state write fails after the first candidate failed: the
 * boot ends with the candidate's error, not the policy's */
static void test_policy_next_fails_after_a_candidate(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;
    fake.next_fail_call = 2;
    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;

    assert_int_equal(boot_load(NULL), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(fake.next_calls, 2);
    assert_int_equal(fake.report_calls, 1);
    /* B was never handed out */
    assert_int_equal(fake.auth_calls, 1);
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

/* No jump without the lockdown (F-244), and a late_boot error comes first */
static void test_lockdown_error(void **state)
{
    (void)state;

    fake_lockdown_rc = -PB_ERR_IO;
    boot_clear_set_flags(0, BOOT_FLAG_CMD);
    assert_int_equal(boot(NULL), -PB_ERR_IO);
    assert_int_equal(fake.late_calls, 1);
    assert_int_equal(fake_lockdown_calls, 1);
    assert_int_equal(fake.jump_calls, 0);
    assert_int_equal(boot_get_flags(), 0);

    fake_lockdown_calls = 0;
    fake.late_rc = -PB_ERR_ABORT;
    assert_int_equal(boot(NULL), -PB_ERR_ABORT);
    assert_int_equal(fake_lockdown_calls, 0);
}

/* The whole disk, to see that a boot in place writes nothing */
static uint8_t *disk_copy(struct fixture *f)
{
    uint8_t *copy = malloc(DISK_BLOCKS * RAMDISK_BLOCK_SIZE);

    assert_non_null(copy);
    memcpy(copy, f->disk->data, DISK_BLOCKS * RAMDISK_BLOCK_SIZE);
    return copy;
}

/* What every boot in place has in common: verified where it is */
static void assert_verified_in_place(struct fixture *f, unsigned int reads_before)
{
    /* The flash is written past the cache: what the cache holds of the
     * partition, its header too, is dropped once, before anything of it is
     * read */
    assert_int_equal(fake.invalidate_calls, 1);
    assert_true(fake.invalidate_start == (uintptr_t)sys_start(f, 0));
    assert_int_equal(fake.invalidate_len, SYS_SIZE);
    assert_int_equal(fake.invalidate_after_auth_calls, 0);

    /* The partition's mapped range, without the header at its end */
    assert_int_equal(fake.in_place_calls, 1);
    assert_true(fake.in_place_base == (uintptr_t)sys_start(f, 0));
    assert_int_equal(fake.in_place_size, SYS_SIZE - sizeof(struct bpak_header));
    /* Not the check for a part that is loaded */
    assert_int_equal(fake.verify_parts_calls, 0);

    /* Nothing is read: the parts are hashed where they are */
    assert_int_equal(fake.load_calls, 1);
    assert_null(fake.load_read_f);
    assert_null(fake.load_result_f);
    assert_int_equal(f->disk->reads, reads_before);
    assert_int_equal(fake.verify_payload_calls, 1);

    /* The header is the one at the end of the partition, as a copy of pb's:
     * every step and the boot driver get the same one */
    assert_int_equal(fake.auth_calls, 1);
    assert_memory_equal(&auth_seen, sys_header(f, 0), sizeof(auth_seen));
    assert_true((uint8_t *)fake.auth_hdr < f->disk->data ||
                (uint8_t *)fake.auth_hdr >= f->disk->data + DISK_BLOCKS * RAMDISK_BLOCK_SIZE);
    assert_ptr_equal(fake.in_place_hdr, fake.auth_hdr);
    assert_ptr_equal(fake.load_hdr, fake.auth_hdr);
    assert_ptr_equal(fake.prepare_hdr, fake.auth_hdr);
}

/* The policy hands out a partition that the board's table says is booted in
 * place (board.md 2.4, boot.md 4.1) */
static void test_in_place_boot(void **state)
{
    struct fixture *f = *state;
    uint8_t *before = disk_copy(f);
    unsigned int reads = f->disk->reads;

    assert_int_equal(boot(NULL), -PB_ERR);

    assert_verified_in_place(f, reads);
    assert_int_equal(fake.prepare_calls, 1);
    assert_memory_equal(fake.prepare_uu, UU_SYS_A, 16);
    assert_int_equal(fake.late_calls, 1);
    assert_int_equal(fake_lockdown_calls, 1);
    assert_int_equal(fake.jump_calls, 1);

    /* The fake auth cleared the signature in its header, as bpak does: that
     * was pb's copy, the partition is as it was */
    assert_memory_equal(f->disk->data, before, DISK_BLOCKS * RAMDISK_BLOCK_SIZE);
    assert_int_equal(f->disk->writes, 0);
    free(before);
}

/* A partition the host names (boot part) is in place all the same */
static void test_in_place_boot_override(void **state)
{
    struct fixture *f = *state;
    unsigned int reads = f->disk->reads;

    /* Not the policy's */
    fake.no_of_candidates = 0;
    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), PB_OK);
    assert_int_equal(fake.next_calls, 0);
    assert_int_equal(fake.report_calls, 0);

    assert_verified_in_place(f, reads);
    assert_int_equal(fake.prepare_calls, 1);
    assert_memory_equal(fake.prepare_uu, UU_SYS_A, 16);
}

/* In place is the partition's property, not the table's: B is loaded */
static void test_in_place_other_partition_is_loaded(void **state)
{
    struct fixture *f = *state;
    unsigned int reads = f->disk->reads;

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);

    assert_int_equal(fake.in_place_calls, 0);
    assert_int_equal(fake.invalidate_calls, 0);
    assert_int_equal(fake.verify_parts_calls, 1);
    assert_non_null(fake.load_read_f);
    /* Its header came through bio */
    assert_int_equal(f->disk->reads, reads + 1);
    assert_memory_equal(&auth_seen, sys_header(f, 1), sizeof(auth_seen));
    assert_memory_equal(fake.prepare_uu, UU_SYS_B, 16);
}

static void test_in_place_not_bootable(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->sys_a, BIO_FLAG_BOOTABLE, 0), PB_OK);
    assert_int_equal(boot_load(NULL), -PB_ERR_PART_NOT_BOOTABLE);

    /* Refused before its header is looked at */
    assert_int_equal(fake.invalidate_calls, 0);
    assert_int_equal(fake.auth_calls, 0);
    assert_int_equal(fake.in_place_calls, 0);
    assert_int_equal(fake.load_calls, 0);
    assert_int_equal(fake.prepare_calls, 0);
}

static void test_in_place_boot_errors(void **state)
{
    (void)state;

    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;
    assert_int_equal(boot_load(NULL), -PB_ERR_AUTHENTICATION_FAILED);
    /* Placement comes after the signature */
    assert_int_equal(fake.in_place_calls, 0);
    /* The cache was dropped before the header that failed was read */
    assert_int_equal(fake.invalidate_calls, 1);
    assert_int_equal(fake.invalidate_after_auth_calls, 0);
    fake.auth_rc = PB_OK;

    /* A part outside the partition's mapped range: nothing is hashed */
    fake.in_place_rc = -PB_ERR_MEM;
    assert_int_equal(boot_load(NULL), -PB_ERR_MEM);
    assert_int_equal(fake.load_calls, 0);
    fake.in_place_rc = PB_OK;

    fake.load_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    assert_int_equal(fake.verify_payload_calls, 0);
    fake.load_rc = PB_OK;

    fake.verify_payload_rc = -PB_ERR_BAD_PAYLOAD;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_PAYLOAD);
    fake.verify_payload_rc = PB_OK;

    assert_int_equal(fake.prepare_calls, 0);

    fake.prepare_rc = -PB_ERR_BAD_META;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_META);
    fake.prepare_rc = PB_OK;

    /* The loading path's check has no say in place */
    fake.verify_parts_rc = -PB_ERR_MEM;
    assert_int_equal(boot_load(NULL), PB_OK);

    /* Every attempt drops the cache again, before its own header: a host can
     * have written the partition between two of them */
    assert_int_equal(fake.invalidate_calls, 6);
    assert_int_equal(fake.invalidate_after_auth_calls, 5);
    assert_int_equal(fake.auth_calls, 6);

    assert_int_equal(fake.verify_parts_calls, 0);
    assert_int_equal(fake.late_calls, 0);
    assert_int_equal(fake.jump_calls, 0);
}

/* While A exists it is booted in place: no other table takes its table's
 * place, which would have it loaded, to its address in the flash */
static void test_in_place_stays_in_place(void **state)
{
    struct fixture *f = *state;
    unsigned int reads = f->disk->reads;
    bio_dev_t disk = bio_get_part_by_uu(UU_DISK);

    assert_true(disk >= 0);
    assert_int_equal(static_ptbl_init_mapped(disk, 0, nothing_mapped), -PB_ERR_STATE);
    assert_int_equal(static_ptbl_init_mapped(disk, (uintptr_t)f->disk->data, mapped_table),
                     -PB_ERR_STATE);

    assert_int_equal(boot_load(NULL), PB_OK);
    assert_verified_in_place(f, reads);
}

/* A partition without room for a header: nothing before it is read as one */
static void test_in_place_smaller_than_a_header(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->tiny, 0, BIO_FLAG_BOOTABLE), PB_OK);
    assert_int_equal(boot_load((unsigned char *)UU_TINY), -PB_ERR_BAD_HEADER);

    /* The size is checked first: no range that the partition doesn't have */
    assert_int_equal(fake.invalidate_calls, 0);
    assert_int_equal(fake.auth_calls, 0);
    assert_int_equal(fake.in_place_calls, 0);
    assert_int_equal(fake.load_calls, 0);
    assert_int_equal(fake.prepare_calls, 0);
}

/* The host's stream is loaded, whatever the table says of the partition the
 * host names with it */
static void test_in_place_cb_boot_is_loaded(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);

    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), PB_OK);
    assert_int_equal(fake.in_place_calls, 0);
    assert_int_equal(fake.invalidate_calls, 0);
    assert_int_equal(fake.verify_parts_calls, 1);
    assert_ptr_equal(fake.load_read_f, fake_read_cb);
    assert_memory_equal(fake.prepare_uu, UU_SYS_A, 16);
}

/* Runs after the mapped cases, with the same UUIDs: a table that was mapped
 * for another disk says nothing about this one */
static void test_bio_boot_after_in_place(void **state)
{
    struct fixture *f = *state;
    unsigned int reads = f->disk->reads;

    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.in_place_calls, 0);
    assert_int_equal(fake.invalidate_calls, 0);
    assert_int_equal(fake.verify_parts_calls, 1);
    assert_non_null(fake.load_read_f);
    assert_int_equal(f->disk->reads, reads + 1);
}

static void test_cb_boot(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);

    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);
    /* The header comes first, at a negative offset */
    assert_int_equal(fake.read_cb_calls, 1);
    assert_int_equal(fake.read_cb_offset, -(int64_t)sizeof(struct bpak_header));
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

#define BOOT_TEST(f)   cmocka_unit_test_setup_teardown(f, setup, teardown)
#define MAPPED_TEST(f) cmocka_unit_test_setup_teardown(f, setup_mapped, teardown)

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
        BOOT_TEST(test_bio_boot_needs_a_policy),
        BOOT_TEST(test_policy_next_candidate),
        BOOT_TEST(test_policy_next_candidate_after_prepare),
        BOOT_TEST(test_policy_every_candidate_fails),
        BOOT_TEST(test_policy_next_fails_after_a_candidate),
        BOOT_TEST(test_early_boot_abort),
        BOOT_TEST(test_late_boot_error),
        BOOT_TEST(test_lockdown_error),
        MAPPED_TEST(test_in_place_boot),
        MAPPED_TEST(test_in_place_boot_override),
        MAPPED_TEST(test_in_place_other_partition_is_loaded),
        MAPPED_TEST(test_in_place_not_bootable),
        MAPPED_TEST(test_in_place_boot_errors),
        MAPPED_TEST(test_in_place_stays_in_place),
        MAPPED_TEST(test_in_place_smaller_than_a_header),
        MAPPED_TEST(test_in_place_cb_boot_is_loaded),
        /* After the mapped cases: what they left mapped is gone */
        BOOT_TEST(test_bio_boot_after_in_place),
        BOOT_TEST(test_cb_boot),
        BOOT_TEST(test_cb_boot_errors),
        BOOT_TEST(test_invalid_default_source),
        BOOT_TEST(test_jump_removed_after_init),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
