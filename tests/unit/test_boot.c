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
#include <bpak/bpak.h>
#include <drivers/partition/static.h>
#include <pb/arch.h>
#include <pb/bio.h>
#include <pb/device_uuid.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
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

/* The steps of a boot where a fake asks for the verified header
 * (boot_image_get_bpak_header(), board.md 5.3) */
enum step {
    AT_EARLY,
    AT_NEXT,
    AT_READ, /* The host's stream */
    AT_RESULT,
    AT_AUTH,
    AT_PLACEMENT,
    AT_LOAD,
    AT_PAYLOAD,
    AT_PREPARE,
    AT_DELIVERY_PREPARE,
    AT_REPORT,
    AT_LATE,
    AT_JUMP,
    AT_END,
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
    /* This call of the payload's check fails with -PB_ERR_BAD_PAYLOAD, 0: none */
    int verify_payload_fail_call;
    int load_calls;
    struct bpak_header *load_hdr;
    boot_read_cb_t load_read_f;
    boot_result_cb_t load_result_f;

    int early_rc;
    int early_calls;

    /* pb's information: what the device answers, and when it was asked */
    int device_uuid_rc;
    int device_uuid_calls;
    int early_calls_at_device_uuid;
    int next_calls_at_device_uuid;
    slc_t slc;
    int slc_calls;
    /* The keystore's keys are key_ids[]. A bit per index: revoked, and a
     * status that can't be read */
    size_t no_of_keys;
    int no_of_keys_rc; /* The RoT's error: it isn't set up */
    int no_of_keys_calls;
    uint32_t revoked_keys;
    uint32_t unreadable_keys;

    /* The fake policy: it hands out candidates[], then -PB_ERR_NOT_FOUND */
    const unsigned char *candidates[3];
    /* How it chose each of them. Like A/B, it keeps the answer of the
     * candidate it handed out last, also when it has none left */
    enum boot_policy_mode modes[3];
    enum boot_policy_mode mode;
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
    /* pb's information as prepare got it, the first three times and the
     * last, and where it was */
    struct boot_info prepare_info[3];
    struct boot_info last_info;
    const struct boot_info *prepare_info_at;
    /* The entry prepare() got was all zero, every time */
    bool prepare_got_zeros;
    /* Whether the candidate was in place, as each of the first three
     * prepares was told (boot_image_in_place()) */
    bool prepare_in_place[3];
    int late_rc;
    int late_calls;
    uint32_t late_flags;
    /* The jump: arch_jump()'s calls and arguments, and what came before it */
    int jump_calls;
    /* prepare fails for this partition */
    const unsigned char *prepare_fail_uu;
    void *jump_args[5];
    int cleaned_calls;
    uintptr_t cleaned_start;
    size_t cleaned_len;
    int mmu_disabled;
    int mmu_disabled_at_clean;
    int mmu_disabled_at_jump;
    int lockdown_calls_at_jump;
    /* The watchdog's kicks so far, at late_boot and at either jump */
    unsigned int kicks_at_late;
    unsigned int kicks_at_jump;
    /* The delivery, on a board that has one */
    int delivery_prepare_rc;
    int delivery_prepare_calls;
    int delivery_prepare_after;
    struct bpak_header *delivery_prepare_hdr;
    struct boot_handover_entry delivery_prepare_next;
    int delivery_jump_calls;
    struct boot_handover_entry delivery_jump_next;

    int read_cb_rc;
    int read_cb_calls;
    int64_t read_cb_offset;
    int result_cb_calls;
    int last_result;

    /* The verified header, asked for at each step: how often, how often
     * there was one, and the last answer */
    int hdr_asked[AT_END];
    int hdr_given[AT_END];
    const struct bpak_header *hdr_at[AT_END];
    /* auth_calls when a payload's hash was last checked and right, -1: never */
    int payload_ok_at_auth_call;
    /* A header was given that isn't the one authenticated last, or whose
     * payload hash wasn't checked */
    int hdr_unverified;
    /* A header was given while a failed candidate was reported */
    int hdr_given_at_failed_report;
} fake;

/* The header as boot_image_auth_header() got it, and as it left it */
static struct bpak_header auth_seen;
static struct bpak_header auth_left;
/* The verified header's bytes, as last given at each step */
static struct bpak_header hdr_seen[AT_END];

/* What the board would get if it asked for the verified header now */
static void see_header(enum step at)
{
    const struct bpak_header *hdr = boot_image_get_bpak_header();

    fake.hdr_asked[at]++;
    fake.hdr_at[at] = hdr;

    if (hdr == NULL)
        return;

    fake.hdr_given[at]++;
    memcpy(&hdr_seen[at], hdr, sizeof(*hdr));

    if (fake.payload_ok_at_auth_call != fake.auth_calls ||
        memcmp(hdr, &auth_left, sizeof(auth_left)) != 0)
        fake.hdr_unverified++;
}

/* test_boot links no arch. The cache calls of the flow: the one that drops
 * what the cache holds of a partition that is verified in place, and the
 * direct delivery's before the jump */
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
    memcpy(&auth_left, hdr, sizeof(auth_left));
    see_header(AT_AUTH);
    return fake.auth_rc;
}

int boot_image_verify_parts(struct bpak_header *hdr)
{
    (void)hdr;
    see_header(AT_PLACEMENT);
    fake.verify_parts_calls++;
    return fake.verify_parts_rc;
}

int boot_image_verify_parts_in_place(struct bpak_header *hdr, uintptr_t base, size_t size)
{
    see_header(AT_PLACEMENT);
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
    see_header(AT_LOAD);
    fake.load_calls++;
    fake.load_hdr = hdr;
    fake.load_read_f = read_f;
    fake.load_result_f = result_f;
    return fake.load_rc;
}

int boot_image_verify_payload(struct bpak_header *hdr, uint8_t *payload_digest)
{
    int rc = fake.verify_payload_rc;

    (void)hdr;
    (void)payload_digest;
    /* The hash isn't checked until this returns */
    see_header(AT_PAYLOAD);
    fake.verify_payload_calls++;

    if (fake.verify_payload_calls == fake.verify_payload_fail_call)
        rc = -PB_ERR_BAD_PAYLOAD;

    if (rc == PB_OK)
        fake.payload_ok_at_auth_call = fake.auth_calls;

    return rc;
}

static int fake_early_boot(void)
{
    see_header(AT_EARLY);
    fake.early_calls++;
    return fake.early_rc;
}

/* What pb's information is read from: the core asks, the handover doesn't
 * (boot.md 5.6) */
#define DEVICE_UUID "02e49231-756e-35ee-a982-378e5ba866a9"

static const key_id_t key_ids[] = { 0xa90f9680, 0x25c6dd36, 0x52c1eda0 };

int device_uuid(uuid_t uu)
{
    fake.device_uuid_calls++;
    fake.early_calls_at_device_uuid = fake.early_calls;
    fake.next_calls_at_device_uuid = fake.next_calls;

    if (fake.device_uuid_rc != PB_OK)
        return fake.device_uuid_rc;

    return uuid_parse(DEVICE_UUID, uu);
}

slc_t slc_read_status(void)
{
    fake.slc_calls++;
    return fake.slc;
}

int rot_no_of_keys(size_t *count)
{
    fake.no_of_keys_calls++;

    if (fake.no_of_keys_rc != PB_OK)
        return fake.no_of_keys_rc;

    *count = fake.no_of_keys;
    return PB_OK;
}

int rot_read_key_status_by_idx(unsigned int index)
{
    if (fake.unreadable_keys & (1u << index))
        return -PB_ERR_IO;

    return (fake.revoked_keys & (1u << index)) ? -PB_ERR_KEY_REVOKED : PB_OK;
}

int rot_key_idx_to_id(unsigned int index, key_id_t *id)
{
    if (!(index < ARRAY_SIZE(key_ids)))
        return -PB_ERR_PARAM;

    *id = key_ids[index];
    return PB_OK;
}

static int fake_policy_init(const struct boot_policy *policy)
{
    (void)policy;
    return PB_OK;
}

static int fake_policy_next(uuid_t part_uu)
{
    see_header(AT_NEXT);
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

    fake.mode = fake.modes[fake.next_candidate];
    uuid_copy(part_uu, fake.candidates[fake.next_candidate++]);
    return PB_OK;
}

static void fake_policy_report(const uuid_t part_uu, int result)
{
    see_header(AT_REPORT);

    if (result != PB_OK && fake.hdr_at[AT_REPORT] != NULL)
        fake.hdr_given_at_failed_report++;

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
    return fake.mode;
}

/* A/B's names: the two systems, and "?" for any other partition */
static const char *fake_policy_system_name(uuid_t part_uu)
{
    if (uuid_compare(part_uu, UU_SYS_A) == 0)
        return "A";

    if (uuid_compare(part_uu, UU_SYS_B) == 0)
        return "B";

    return "?";
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

/* The same policy with names for its systems, as A/B has */
static const struct boot_policy_ops named_policy_ops = {
    .init = fake_policy_init,
    .next = fake_policy_next,
    .report = fake_policy_report,
    .mode = fake_policy_mode,
    .activate = fake_policy_activate,
    .status = fake_policy_status,
    .system_name = fake_policy_system_name,
};

static const struct boot_policy named_policy = { .ops = &named_policy_ops };

/* What the fake handover gives: recognizable in every register */
#define NEXT_ENTRY 0x40400000
#define NEXT_ARG0  0x48000000
#define NEXT_ARG1  0x11111111
#define NEXT_ARG2  0x22222222
#define NEXT_ARG3  0x33333333
#define NEXT_DT    0x43000000

static int fake_prepare(struct bpak_header *header,
                        const struct boot_info *info,
                        struct boot_handover_entry *next)
{
    static const struct boot_handover_entry zeros;
    const unsigned char *boot_part_uu = info->boot_partition_uuid;

    if (fake.prepare_calls == 0)
        fake.prepare_got_zeros = true;
    if (memcmp(next, &zeros, sizeof(zeros)) != 0)
        fake.prepare_got_zeros = false;

    if (fake.prepare_calls < 3) {
        fake.prepare_info[fake.prepare_calls] = *info;
        fake.prepare_in_place[fake.prepare_calls] = boot_image_in_place();
    }

    fake.last_info = *info;
    fake.prepare_info_at = info;
    fake.prepare_calls++;
    fake.prepare_hdr = header;
    uuid_copy(fake.prepare_uu, boot_part_uu);
    /* Where the board's DT hook runs */
    see_header(AT_PREPARE);

    /* Also when it fails: nothing of a failed prepare may be jumped to */
    next->entry = NEXT_ENTRY;
    next->arg[0] = NEXT_ARG0;
    next->arg[1] = NEXT_ARG1;
    next->arg[2] = NEXT_ARG2;
    next->arg[3] = NEXT_ARG3;
    next->dt = NEXT_DT;

    if (fake.prepare_fail_uu != NULL && uuid_compare(boot_part_uu, fake.prepare_fail_uu) == 0)
        return -PB_ERR_BAD_META;

    return fake.prepare_rc;
}

static int fake_delivery_prepare(struct bpak_header *header, const struct boot_handover_entry *next)
{
    see_header(AT_DELIVERY_PREPARE);
    fake.delivery_prepare_calls++;
    fake.delivery_prepare_after = fake.prepare_calls;
    fake.delivery_prepare_hdr = header;
    fake.delivery_prepare_next = *next;
    return fake.delivery_prepare_rc;
}

/* A delivery's jump returns when it failed, which boot_jump() reports */
static void fake_delivery_jump(const struct boot_handover_entry *next)
{
    see_header(AT_JUMP);
    fake.delivery_jump_calls++;
    fake.delivery_jump_next = *next;
    fake.kicks_at_jump = fake_wdog_kicks;
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    fake.cleaned_calls++;
    fake.cleaned_start = start;
    fake.cleaned_len = len;
    fake.mmu_disabled_at_clean = fake.mmu_disabled;
}

void arch_disable_mmu(void)
{
    fake.mmu_disabled++;
}

/* arch_jump() doesn't return: the fake leaves through boot_to_the_jump() */
static jmp_buf jump_env;
#define JUMPED 1

void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3)
{
    see_header(AT_JUMP);
    fake.jump_calls++;
    fake.jump_args[0] = addr;
    fake.jump_args[1] = p0;
    fake.jump_args[2] = p1;
    fake.jump_args[3] = p2;
    fake.jump_args[4] = p3;
    fake.mmu_disabled_at_jump = fake.mmu_disabled;
    fake.lockdown_calls_at_jump = fake_lockdown_calls;
    fake.kicks_at_jump = fake_wdog_kicks;
    longjmp(jump_env, 1);
}

/* boot(), on a board whose core jumps itself: JUMPED when it got there,
 * else what boot() returned */
static int boot_to_the_jump(uuid_t boot_part_override_uu)
{
    if (setjmp(jump_env) == 0)
        return boot(boot_part_override_uu);

    return JUMPED;
}

/* The hook gets no header (D-047): a board that needs it asks for it */
static int fake_late_boot(uuid_t boot_part_uu)
{
    (void)boot_part_uu;
    see_header(AT_LATE);
    fake.late_calls++;
    fake.late_flags = boot_get_flags();
    fake.kicks_at_late = fake_wdog_kicks;
    return fake.late_rc;
}

/* What the host's stream gives as its header */
static void stream_header(uint8_t *buf, size_t length)
{
    for (size_t i = 0; i < length; i++)
        buf[i] = (uint8_t)(i * 5 + 3);
}

static int fake_read_cb(int64_t offset, size_t length, void *buf)
{
    see_header(AT_READ);
    fake.read_cb_calls++;
    fake.read_cb_offset = offset;
    stream_header(buf, length);
    return fake.read_cb_rc;
}

static int fake_result_cb(int result)
{
    see_header(AT_RESULT);
    fake.result_cb_calls++;
    fake.last_result = result;
    return PB_OK;
}

static const struct boot_handover_ops fake_handover_ops = {
    .prepare = fake_prepare,
};
static const struct boot_handover fake_handover = { .ops = &fake_handover_ops };

static const struct boot_delivery_ops fake_delivery_ops = {
    .prepare = fake_delivery_prepare,
    .jump = fake_delivery_jump,
};
static const struct boot_delivery fake_delivery = { .ops = &fake_delivery_ops };

/* A board without a delivery: the core jumps itself */
static const struct boot_driver driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .early_boot_cb = fake_early_boot,
    .policy = &fake_policy,
    .handover = &fake_handover,
    .late_boot_cb = fake_late_boot,
};

/* The same board with a policy that names its systems */
static const struct boot_driver named_driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .early_boot_cb = fake_early_boot,
    .policy = &named_policy,
    .handover = &fake_handover,
    .late_boot_cb = fake_late_boot,
};

/* The same board with a delivery */
static const struct boot_driver delivery_driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .policy = &fake_policy,
    .handover = &fake_handover,
    .delivery = &fake_delivery,
    .late_boot_cb = fake_late_boot,
};

/* Only what boot_init() requires: no policy */
static const struct boot_driver minimal_driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .handover = &fake_handover,
};

/* A board without a policy has the object, without ops */
static const struct boot_policy no_policy;

static const struct boot_driver no_policy_driver = {
    .default_boot_source = BOOT_SOURCE_BIO,
    .policy = &no_policy,
    .handover = &fake_handover,
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

/* What a header would be, at the end of each system: A's and B's differ */
static void fill_headers(struct fixture *f)
{
    uint8_t *hdr_a = sys_header(f, 0);
    uint8_t *hdr_b = sys_header(f, 1);

    for (size_t i = 0; i < sizeof(struct bpak_header); i++) {
        hdr_a[i] = (uint8_t)(i * 7 + 1);
        hdr_b[i] = (uint8_t)(i * 13 + 2);
    }
}

static void reset_fakes(struct fixture *f)
{
    memset(&fake, 0, sizeof(fake));
    memset(&auth_seen, 0, sizeof(auth_seen));
    memset(&auth_left, 0, sizeof(auth_left));
    memset(hdr_seen, 0, sizeof(hdr_seen));
    fake.payload_ok_at_auth_call = -1;
    fill_headers(f);
    /* The policy's one candidate is System A */
    fake.candidates[0] = UU_SYS_A;
    fake.no_of_candidates = 1;
    /* A locked device, none of its three keys revoked */
    fake.slc = SLC_CONFIGURATION_LOCKED;
    fake.no_of_keys = ARRAY_SIZE(key_ids);
    fake_lockdown_calls = 0;
    fake_lockdown_rc = PB_OK;
    fake_lockdown_wdog_kicks = 0;
    fake_wdog_kicks = 0;
    /* Nothing of the platform's failed */
    fake_plat_status = (struct plat_status)FAKE_PLAT_STATUS_DEFAULT;
    fake_plat_status_calls = 0;
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

    reset_fakes(f);
    *state = f;
    return 0;
}

/*
 * Nothing unverified is handed out (board.md 5.3, security.md 4.2): no header
 * before the candidate's payload hash was checked, none of a candidate that
 * failed, and never another than the one that was authenticated last.
 */
static void assert_no_unverified_header(void)
{
    static const enum step before_the_hash[] = {
        AT_EARLY, AT_NEXT, AT_READ, AT_AUTH, AT_PLACEMENT, AT_LOAD, AT_PAYLOAD,
    };

    for (size_t i = 0; i < sizeof(before_the_hash) / sizeof(before_the_hash[0]); i++)
        assert_int_equal(fake.hdr_given[before_the_hash[i]], 0);

    assert_int_equal(fake.hdr_given_at_failed_report, 0);
    assert_int_equal(fake.hdr_unverified, 0);
}

/* It holds in every case here, whatever the case is about */
static int teardown(void **state)
{
    struct fixture *f = *state;

    assert_no_unverified_header();
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
    assert_null(boot_image_get_bpak_header());
    assert_int_equal(boot_set_boot_partition((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);
    memset(uu, 0xff, sizeof(uu));
    boot_get_boot_partition(uu);
    assert_true(uuid_is_null(uu));
}

static void test_init_rejects(void **state)
{
    /* A handover is mandatory: the object, its functions, its prepare */
    static const struct boot_driver no_handover = { .default_boot_source = BOOT_SOURCE_BIO };
    static const struct boot_handover no_ops = { .ops = NULL };
    static const struct boot_driver handover_without_ops = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .handover = &no_ops,
    };
    static const struct boot_handover_ops no_prepare_ops = { .prepare = NULL };
    static const struct boot_handover no_prepare = { .ops = &no_prepare_ops };
    static const struct boot_driver handover_without_prepare = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .handover = &no_prepare,
    };
    /* A delivery isn't, but one that is there can jump */
    static const struct boot_delivery delivery_no_ops = { .ops = NULL };
    static const struct boot_driver delivery_without_ops = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .handover = &fake_handover,
        .delivery = &delivery_no_ops,
    };
    static const struct boot_delivery_ops no_jump_ops = { .prepare = fake_delivery_prepare };
    static const struct boot_delivery delivery_no_jump = { .ops = &no_jump_ops };
    static const struct boot_driver delivery_without_jump = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .handover = &fake_handover,
        .delivery = &delivery_no_jump,
    };
    (void)state;

    assert_int_equal(boot_init(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_init(&no_handover), -PB_ERR_PARAM);
    assert_int_equal(boot_init(&handover_without_ops), -PB_ERR_PARAM);
    assert_int_equal(boot_init(&handover_without_prepare), -PB_ERR_PARAM);
    assert_int_equal(boot_init(&delivery_without_ops), -PB_ERR_PARAM);
    assert_int_equal(boot_init(&delivery_without_jump), -PB_ERR_PARAM);
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
    assert_int_equal(boot_to_the_jump(NULL), JUMPED);

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
    /* The jump, once */
    assert_int_equal(fake.jump_calls, 1);
    assert_int_equal(fake.lockdown_calls_at_jump, 1);
    /* The watchdog is kicked once before the jump (platform.md 3.3, F-243):
     * after late_boot and the lockdown, so that the next stage has the whole
     * timeout. The chunks' kicks are boot_image_load_and_hash()'s, which is
     * a fake here */
    assert_int_equal(fake.kicks_at_late, 0);
    assert_int_equal(fake_lockdown_wdog_kicks, 0);
    assert_int_equal(fake.kicks_at_jump, 1);
    assert_int_equal(fake_wdog_kicks, 1);
    /* Directly (boot.md 5.1): the handover's entry and its four arguments,
     * with the cache cleaned and off. The entry itself is cleaned first, it
     * is read after the cache went off */
    assert_true(fake.prepare_got_zeros);
    assert_ptr_equal(fake.jump_args[0], (void *)(uintptr_t)NEXT_ENTRY);
    assert_ptr_equal(fake.jump_args[1], (void *)(uintptr_t)NEXT_ARG0);
    assert_ptr_equal(fake.jump_args[2], (void *)(uintptr_t)NEXT_ARG1);
    assert_ptr_equal(fake.jump_args[3], (void *)(uintptr_t)NEXT_ARG2);
    assert_ptr_equal(fake.jump_args[4], (void *)(uintptr_t)NEXT_ARG3);
    assert_int_equal(fake.cleaned_calls, 1);
    assert_int_equal(fake.cleaned_len, sizeof(struct boot_handover_entry));
    assert_int_equal(fake.mmu_disabled_at_clean, 0);
    assert_int_equal(fake.mmu_disabled, 1);
    assert_int_equal(fake.mmu_disabled_at_jump, 1);
    assert_int_equal(fake.delivery_jump_calls, 0);
    /* A partition that is loaded is read through bio, not through a map:
     * nothing of it is in the cache */
    assert_int_equal(fake.invalidate_calls, 0);
    /* The flags are there to the jump: a delivery whose jump comes back
     * clears them (test_delivery) */
    assert_int_equal(boot_get_flags(), BOOT_FLAG_VERBOSE);

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
    /* The kick before the jump is boot_jump()'s */
    assert_int_equal(fake_wdog_kicks, 0);
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

/* A loaded partition without room for a header: refused as in place, before
 * anything is read (F-279) */
static void test_bio_boot_smaller_than_a_header(void **state)
{
    struct fixture *f = *state;
    bio_dev_t disk = bio_get_part_by_uu(UU_DISK);
    bio_dev_t tiny = bio_allocate_parent(disk,
                                         2 * SYS_BLOCKS + SYS_A_FIRST_BLOCK,
                                         2 * SYS_BLOCKS + SYS_A_FIRST_BLOCK + 6,
                                         RAMDISK_BLOCK_SIZE,
                                         UU_TINY,
                                         "Tiny");
    unsigned int reads_before = f->disk->reads;

    assert_true(tiny >= 0);
    assert_int_equal(bio_clear_set_flags(tiny, 0, BIO_FLAG_BOOTABLE), PB_OK);
    assert_int_equal(boot_load((unsigned char *)UU_TINY), -PB_ERR_BAD_HEADER);

    assert_int_equal(f->disk->reads, reads_before);
    assert_int_equal(fake.auth_calls, 0);
    assert_int_equal(fake.load_calls, 0);
    assert_int_equal(fake.prepare_calls, 0);
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

    /* Nor does the driver with only what boot_init() requires */
    assert_int_equal(boot_init(&minimal_driver), PB_OK);
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 2);
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

    assert_int_equal(boot_to_the_jump(NULL), JUMPED);

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
    /* One kick before the jump, whichever candidate it is: the loop over
     * the candidates has none of its own (platform.md 3.3 names none) */
    assert_int_equal(fake.kicks_at_late, 0);
    assert_int_equal(fake.kicks_at_jump, 1);
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
    /* No jump, no kick: command mode's loop has the next one */
    assert_int_equal(fake_wdog_kicks, 0);

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

/* The id of the key a system's image is signed with, in its header on the
 * disk */
static void set_key_id(struct fixture *f, int system, uint32_t key_id)
{
    memcpy(sys_header(f, system) + offsetof(struct bpak_header, key_id), &key_id, sizeof(key_id));
}

static void assert_device_info(const struct boot_info *info)
{
    uuid_t device_uu;

    assert_int_equal(uuid_parse(DEVICE_UUID, device_uu), 0);
    assert_memory_equal(info->device_uuid, device_uu, sizeof(uuid_t));
    /* The string the wire's version command answers */
    assert_string_equal(info->version, PB_VERSION);
    assert_int_equal(info->slc, SLC_CONFIGURATION_LOCKED);
    /* Nothing is reported: the platform's preconditions hold */
    assert_int_equal(info->status, 0);
}

/* boot.md 2, 5.6: pb's information is collected once, after early_boot()
 * and before the first candidate, and every candidate's prepare gets it */
static void test_info_is_collected_once_before_the_candidates(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;
    fake.prepare_fail_uu = UU_SYS_A;
    /* The second key is revoked */
    fake.revoked_keys = 1u << 1;

    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 2);

    assert_int_equal(fake.device_uuid_calls, 1);
    assert_int_equal(fake.slc_calls, 1);
    assert_int_equal(fake.no_of_keys_calls, 1);
    assert_int_equal(fake.early_calls_at_device_uuid, 1);
    assert_int_equal(fake.next_calls_at_device_uuid, 0);

    for (int i = 0; i < 2; i++) {
        const struct boot_info *info = &fake.prepare_info[i];

        assert_device_info(info);
        /* The keys that aren't revoked, in the keystore's order */
        assert_int_equal(info->no_of_available_keys, 2);
        assert_int_equal(info->available_keys[0], key_ids[0]);
        assert_int_equal(info->available_keys[1], key_ids[2]);
    }

    /* A boot that command mode starts later collects it again: the keys
     * and the life cycle may have changed (a key was revoked) */
    fake.prepare_fail_uu = NULL;
    fake.revoked_keys = (1u << 0) | (1u << 1);
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.slc_calls, 2);
    assert_int_equal(fake.last_info.no_of_available_keys, 1);
    assert_int_equal(fake.last_info.available_keys[0], key_ids[2]);
}

/* A key whose status can't be read is unusable (security.md 4.4): it isn't
 * among the keys that aren't revoked, and the boot goes on */
static void test_info_key_that_cannot_be_read(void **state)
{
    (void)state;

    fake.unreadable_keys = 1u << 0;
    fake.revoked_keys = 1u << 2;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.no_of_available_keys, 1);
    assert_int_equal(fake.last_info.available_keys[0], key_ids[1]);

    /* No key left: an empty list. No image verifies then, which is the
     * load's to find */
    fake.unreadable_keys = 0;
    fake.revoked_keys = 7;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.no_of_available_keys, 0);
}

/* A board without an SLC boots, and has no life cycle to tell of */
static void test_info_without_an_slc(void **state)
{
    (void)state;

    fake.slc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.slc, BOOT_INFO_NO_SLC);

    /* Every state is the wire's value, also the first */
    fake.slc = SLC_NOT_CONFIGURED;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.slc, SLC_NOT_CONFIGURED);
}

/* boot.md 2: pb's information that can't be read is no candidate's fault
 * and stops the boot, with every source: no candidate is asked for, nothing
 * is read or prepared. A life cycle that can't be read doesn't boot, with
 * any handover (security.md 4.4, system.md 4) */
static void test_info_that_cannot_be_read_stops_the_boot(void **state)
{
    struct fixture *f = *state;

    boot_clear_set_flags(0, BOOT_FLAG_CMD);
    fake.slc = -PB_ERR_IO;
    assert_int_equal(boot(NULL), -PB_ERR_IO);
    assert_int_equal(boot_get_flags(), 0);

    /* Not set up is unknown too, not "no SLC" */
    fake.slc = -PB_ERR_STATE;
    assert_int_equal(boot_load(NULL), -PB_ERR_STATE);
    /* A partition the host named */
    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), -PB_ERR_STATE);
    /* The host's stream: its header isn't asked for */
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    assert_int_equal(boot_load(NULL), -PB_ERR_STATE);
    assert_int_equal(fake.read_cb_calls, 0);
    assert_int_equal(boot_set_source(BOOT_SOURCE_BIO), PB_OK);
    fake.slc = SLC_CONFIGURATION_LOCKED;

    fake.device_uuid_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    fake.device_uuid_rc = PB_OK;

    /* A RoT that isn't set up: its error, which is no number of keys (F-346) */
    fake.no_of_keys_rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
    fake.no_of_keys_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    fake.no_of_keys_rc = PB_OK;

    /* More keys than pb's information has room for */
    fake.no_of_keys = BOOT_INFO_MAX_KEYS + 1;
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
    fake.no_of_keys = ARRAY_SIZE(key_ids);

    /* early_boot() ran each time, and nothing after it */
    assert_int_equal(fake.early_calls, 8);
    assert_int_equal(fake.next_calls, 0);
    assert_int_equal(fake.report_calls, 0);
    assert_int_equal(fake.auth_calls, 0);
    assert_int_equal(f->disk->reads, 0);
    assert_int_equal(fake.prepare_calls, 0);
    assert_int_equal(fake.late_calls, 0);
    assert_int_equal(fake.jump_calls, 0);

    /* A stop in early_boot() comes first: nothing is collected */
    fake.early_rc = -PB_ERR_ABORT;
    fake.device_uuid_calls = 0;
    assert_int_equal(boot_load(NULL), -PB_ERR_ABORT);
    assert_int_equal(fake.device_uuid_calls, 0);
    fake.early_rc = PB_OK;

    /* And it boots when it can be read again */
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_device_info(&fake.last_info);
}

/* boot.md 5.6: the candidate's part of pb's information. The partition and
 * the image's key are the candidate's, the system's name is the policy's */
static void test_info_of_the_candidate(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(boot_init(&named_driver), PB_OK);
    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    set_key_id(f, 0, key_ids[0]);
    set_key_id(f, 1, key_ids[2]);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;
    fake.prepare_fail_uu = UU_SYS_A;

    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 2);

    assert_memory_equal(fake.prepare_info[0].boot_partition_uuid, UU_SYS_A, 16);
    assert_string_equal(fake.prepare_info[0].system_name, "A");
    assert_int_equal(fake.prepare_info[0].active_key, key_ids[0]);

    /* Nothing of the candidate that failed is left in the next one's */
    assert_memory_equal(fake.prepare_info[1].boot_partition_uuid, UU_SYS_B, 16);
    assert_string_equal(fake.prepare_info[1].system_name, "B");
    assert_int_equal(fake.prepare_info[1].active_key, key_ids[2]);

    /* A partition the host named (boot.md 6), in the core's one structure */
    const struct boot_info *at = fake.prepare_info_at;

    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);
    assert_ptr_equal(fake.prepare_info_at, at);
    assert_memory_equal(fake.last_info.boot_partition_uuid, UU_SYS_B, 16);
    assert_string_equal(fake.last_info.system_name, "B");
    assert_int_equal(fake.last_info.active_key, key_ids[2]);
}

/* boot.md 5.6: a boot of the host's stream keeps today's values. The UUID
 * the host names is taken as given, and the system is "?" when it is none
 * of the policy's, or when the host named none */
static void test_info_of_the_hosts_stream(void **state)
{
    uuid_t null_uu;
    (void)state;

    uuid_clear(null_uu);
    assert_int_equal(boot_init(&named_driver), PB_OK);
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);

    /* A system's partition, which isn't read: it need not be bootable */
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);
    assert_memory_equal(fake.last_info.boot_partition_uuid, UU_SYS_B, 16);
    assert_string_equal(fake.last_info.system_name, "B");
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_NORMAL);

    /* Another UUID */
    assert_int_equal(boot_load((unsigned char *)UU_OTHER), PB_OK);
    assert_memory_equal(fake.last_info.boot_partition_uuid, UU_OTHER, 16);
    assert_string_equal(fake.last_info.system_name, "?");

    /* None: all zero */
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_memory_equal(fake.last_info.boot_partition_uuid, null_uu, 16);
    assert_string_equal(fake.last_info.system_name, "?");
    assert_int_equal(boot_load(null_uu), PB_OK);
    assert_memory_equal(fake.last_info.boot_partition_uuid, null_uu, 16);
    assert_device_info(&fake.last_info);
}

/* A policy without systems (fixed, staggered chain) has no names, and a
 * board without a policy has none to ask: no name, the partition says what
 * booted */
static void test_info_without_a_system_name(void **state)
{
    (void)state;

    assert_int_equal(boot_load(NULL), PB_OK);
    assert_null(fake.last_info.system_name);
    assert_memory_equal(fake.last_info.boot_partition_uuid, UU_SYS_A, 16);

    assert_int_equal(boot_init(&no_policy_driver), PB_OK);
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), PB_OK);
    assert_null(fake.last_info.system_name);
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_NORMAL);
}

/* boot.md 3.4, 5.6: the mode of a boot the policy chose is the policy's for
 * the candidate that boots */
static void test_mode_of_the_policys_candidates(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_NORMAL);

    /* The first candidate fails, the policy's second is a fallback: each
     * prepare gets its own candidate's mode */
    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.modes[1] = BOOT_POLICY_MODE_FALLBACK;
    fake.no_of_candidates = 2;
    fake.prepare_fail_uu = UU_SYS_A;
    fake.prepare_calls = 0;

    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 2);
    assert_int_equal(fake.prepare_info[0].boot_mode, BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(fake.prepare_info[1].boot_mode, BOOT_POLICY_MODE_FALLBACK);
    assert_memory_equal(fake.prepare_info[1].boot_partition_uuid, UU_SYS_B, 16);

    /* The next boot's first candidate is normal again */
    fake.prepare_fail_uu = NULL;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_NORMAL);

    /* A/B's speculative mode is a candidate's mode like the others */
    fake.modes[0] = BOOT_POLICY_MODE_SPECULATIVE;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_SPECULATIVE);
}

/* The boot's mode is the core's, not what the policy says when it is asked:
 * after a fallback that failed the policy still answers fallback, and a
 * partition the host names or the host's stream is a normal boot (boot.md
 * 5.6, 6) */
static void test_mode_is_normal_after_a_failed_fallback(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.modes[1] = BOOT_POLICY_MODE_FALLBACK;
    fake.no_of_candidates = 2;
    fake.auth_rc = -PB_ERR_AUTHENTICATION_FAILED;

    /* Both fail: no candidate is left, and pb is in command mode */
    assert_int_equal(boot_load(NULL), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(fake.report_calls, 2);
    assert_int_equal(boot_policy_mode(), BOOT_POLICY_MODE_FALLBACK);
    fake.auth_rc = PB_OK;

    /* 'boot part <uuid>' */
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);
    assert_int_equal(boot_policy_mode(), BOOT_POLICY_MODE_FALLBACK);
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_NORMAL);

    /* 'boot bpak' */
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    assert_int_equal(boot_load((unsigned char *)UU_SYS_B), PB_OK);
    assert_int_equal(boot_policy_mode(), BOOT_POLICY_MODE_FALLBACK);
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(fake.prepare_calls, 2);

    /* A policy boot that succeeded with its fallback, then a named one */
    assert_int_equal(boot_set_source(BOOT_SOURCE_BIO), PB_OK);
    fake.prepare_fail_uu = UU_SYS_A;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_FALLBACK);
    fake.prepare_fail_uu = NULL;
    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), PB_OK);
    assert_int_equal(fake.last_info.boot_mode, BOOT_POLICY_MODE_NORMAL);
}

/* A boot that a precondition refused read nothing and started nothing: not
 * the board's early_boot(), not pb's information, no candidate */
static void assert_refused_before_anything(int rc)
{
    assert_int_equal(rc, -PB_ERR_STATE);
    assert_int_equal(fake.early_calls, 0);
    assert_int_equal(fake.device_uuid_calls, 0);
    assert_int_equal(fake.slc_calls, 0);
    assert_int_equal(fake.next_calls, 0);
    assert_int_equal(fake.read_cb_calls, 0);
    assert_int_equal(fake.auth_calls, 0);
    assert_int_equal(fake.load_calls, 0);
    assert_int_equal(fake.prepare_calls, 0);
    assert_int_equal(fake.late_calls, 0);
    assert_int_equal(fake.jump_calls, 0);
    assert_int_equal(boot_get_flags(), 0);
    assert_null(boot_image_get_bpak_header());
}

/* security.md 4.5, platform.md 2.3 (F-248): no silent boot. A TrustZone DRAM
 * filter that is bypassed refuses the boot before early_boot(); no filter,
 * a later stage's and one that pb set up boot. A state the core doesn't
 * know is a no */
static void test_precondition_dram_filter(void **state)
{
    static const enum plat_tz_dram_filter boots[] = {
        PLAT_TZ_DRAM_NONE,
        PLAT_TZ_DRAM_LATER_STAGE,
        PLAT_TZ_DRAM_ACTIVE,
    };
    struct fixture *f = *state;

    for (size_t i = 0; i < ARRAY_SIZE(boots); i++) {
        reset_fakes(f);
        fake_plat_status.tz_dram_filter = boots[i];
        assert_int_equal(boot_load(NULL), PB_OK);
        assert_int_equal(fake.prepare_calls, 1);
        assert_int_equal(fake.last_info.status, 0);
        /* Asked once in a boot */
        assert_int_equal(fake_plat_status_calls, 1);
    }

    reset_fakes(f);
    fake_plat_status.tz_dram_filter = PLAT_TZ_DRAM_BYPASSED;
    assert_refused_before_anything(boot(NULL));

    reset_fakes(f);
    fake_plat_status.tz_dram_filter = (enum plat_tz_dram_filter)(PLAT_TZ_DRAM_BYPASSED + 1);
    assert_refused_before_anything(boot(NULL));

    /* Checked in every boot: the next one, with the filter in effect, goes on */
    fake_plat_status.tz_dram_filter = PLAT_TZ_DRAM_ACTIVE;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.early_calls, 1);
    assert_int_equal(fake_plat_status_calls, 2);
}

/* boot.md 2: the preconditions are checked in every boot, also one command
 * mode started: the partition the host names and the host's stream, from
 * which nothing is read */
static void test_precondition_in_a_boot_command_mode_started(void **state)
{
    struct fixture *f = *state;

    fake_plat_status.tz_dram_filter = PLAT_TZ_DRAM_BYPASSED;
    boot_clear_set_flags(0, BOOT_FLAG_CMD);
    assert_refused_before_anything(boot_load((unsigned char *)UU_SYS_A));

    reset_fakes(f);
    fake_plat_status.tz_dram_filter = PLAT_TZ_DRAM_BYPASSED;
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    boot_clear_set_flags(0, BOOT_FLAG_CMD);
    assert_refused_before_anything(boot_load(NULL));
    /* The refusal is boot_load()'s: command mode sends it (D-058) */
    assert_int_equal(fake.result_cb_calls, 0);
}

/* security.md 4.5: the same rule in every life cycle state, without a
 * development override. The life cycle isn't even read */
static void test_precondition_in_every_life_cycle_state(void **state)
{
    static const slc_t states[] = {
        SLC_NOT_CONFIGURED,
        SLC_CONFIGURATION,
        SLC_CONFIGURATION_LOCKED,
        -PB_ERR_NOT_SUPPORTED, /* A board without one */
    };
    struct fixture *f = *state;

    for (size_t i = 0; i < ARRAY_SIZE(states); i++) {
        reset_fakes(f);
        fake.slc = states[i];
        fake_plat_status.tz_dram_filter = PLAT_TZ_DRAM_BYPASSED;
        assert_refused_before_anything(boot(NULL));
    }
}

/* platform.md 2.3: a watchdog that didn't arm, in a build that has one,
 * boots and is reported in pb's status (boot.md 5.6): refusing would leave
 * no system at all. A build without CONFIG_ENABLE_WATCHDOG has nothing to
 * report: its watchdog is never armed */
static void test_precondition_watchdog(void **state)
{
    (void)state;

    fake_plat_status.wdog_armed = false;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.early_calls, 1);
    assert_int_equal(fake.prepare_calls, 1);
#ifdef CONFIG_ENABLE_WATCHDOG
    assert_int_equal(fake.last_info.status, BOOT_INFO_STATUS_WATCHDOG_NOT_ARMED);
#else
    assert_int_equal(fake.last_info.status, 0);
#endif

    /* Every candidate's handover gets it, and it is this boot's: nothing is
     * left of it in the next boot */
    fake_plat_status.wdog_armed = true;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.last_info.status, 0);
}

/* platform.md 2.3: a crypto engine that failed refuses the boot, where the
 * platform is the one that sets it up (CONFIG_PLAT_CRYPTO_ENGINE): nothing
 * verifies the image. Without the option the platform reports none, the
 * board sets the engine up (F-247), and the boot goes on */
static void test_precondition_crypto_engine(void **state)
{
    (void)state;

    fake_plat_status.crypto_engine = false;
#ifdef CONFIG_PLAT_CRYPTO_ENGINE
    assert_refused_before_anything(boot(NULL));
#else
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 1);
    assert_int_equal(fake.last_info.status, 0);
#endif
}

/* platform.md 2.3: nothing of the boot's rests on the ROM's logs. Without
 * them the boot goes on and reports nothing */
static void test_precondition_rom_log(void **state)
{
    (void)state;

    fake_plat_status.rom_log = false;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 1);
    assert_int_equal(fake.last_info.status, 0);
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
    /* A boot that late_boot stops doesn't jump: no kick before it (F-243) */
    assert_int_equal(fake_wdog_kicks, 0);
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
    /* The kick comes after the lockdown: none without the jump (F-243) */
    assert_int_equal(fake_wdog_kicks, 0);

    fake_lockdown_calls = 0;
    fake.late_rc = -PB_ERR_ABORT;
    assert_int_equal(boot(NULL), -PB_ERR_ABORT);
    assert_int_equal(fake_lockdown_calls, 0);
    assert_int_equal(fake_wdog_kicks, 0);
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

    assert_int_equal(boot_to_the_jump(NULL), JUMPED);

    assert_verified_in_place(f, reads);
    assert_int_equal(fake.prepare_calls, 1);
    assert_memory_equal(fake.prepare_uu, UU_SYS_A, 16);
    assert_int_equal(fake.late_calls, 1);
    assert_int_equal(fake_lockdown_calls, 1);
    assert_int_equal(fake.jump_calls, 1);
    assert_int_equal(fake.kicks_at_jump, 1);

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

/* F-291: a handover learns whether its candidate is in place, where nothing
 * of a part can be grown or patched (boot_image_in_place()). It is the
 * candidate's: A in place, then B and the host's stream, which are loaded */
static void test_in_place_is_told_to_the_handover(void **state)
{
    struct fixture *f = *state;

    assert_false(boot_image_in_place());

    /* The policy's: A fails in prepare, B is tried */
    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;
    fake.prepare_fail_uu = UU_SYS_A;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 2);
    assert_true(fake.prepare_in_place[0]);
    assert_false(fake.prepare_in_place[1]);
    fake.prepare_fail_uu = NULL;

    /* A partition the host named */
    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), PB_OK);
    assert_true(fake.prepare_in_place[2]);
    assert_true(boot_image_in_place());

    /* The host's stream after it */
    fake.prepare_calls = 0;
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 1);
    assert_false(fake.prepare_in_place[0]);
    assert_int_equal(boot_set_source(BOOT_SOURCE_BIO), PB_OK);

    /* And a boot that stops before a candidate has none in place */
    assert_int_equal(boot_load((unsigned char *)UU_SYS_A), PB_OK);
    assert_true(boot_image_in_place());
    fake.early_rc = -PB_ERR_ABORT;
    assert_int_equal(boot_load(NULL), -PB_ERR_ABORT);
    assert_false(boot_image_in_place());
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

/*
 * The verified header (board.md 5.3, D-047, F-266): late_boot() gets none, a
 * board asks boot_image_get_bpak_header() for it. The fakes ask at every step
 * (see_header()).
 */

/* A header as the authentication leaves it: bpak clears the signature */
static void as_authenticated(const uint8_t *raw, struct bpak_header *hdr)
{
    memcpy(hdr, raw, sizeof(*hdr));
    memset(hdr->signature, 0, sizeof(hdr->signature));
    hdr->signature_sz = 0;
}

/* From prepare on it is 'verified', pb's own copy, the one every step got.
 * 'late': the boot got to late_boot() and the jump */
static void assert_header_given(const struct bpak_header *verified, bool late)
{
    assert_no_unverified_header();

    assert_true(fake.hdr_asked[AT_AUTH] > 0);
    assert_true(fake.hdr_asked[AT_PLACEMENT] > 0);
    assert_true(fake.hdr_asked[AT_LOAD] > 0);
    assert_true(fake.hdr_asked[AT_PAYLOAD] > 0);

    assert_true(fake.hdr_given[AT_PREPARE] > 0);
    assert_ptr_equal(fake.hdr_at[AT_PREPARE], fake.prepare_hdr);
    assert_ptr_equal(fake.hdr_at[AT_PREPARE], fake.auth_hdr);
    assert_memory_equal(&hdr_seen[AT_PREPARE], verified, sizeof(*verified));

    if (!late) {
        assert_int_equal(fake.hdr_asked[AT_LATE], 0);
        return;
    }

    assert_int_equal(fake.hdr_asked[AT_LATE], 1);
    assert_int_equal(fake.hdr_given[AT_LATE], 1);
    assert_ptr_equal(fake.hdr_at[AT_LATE], fake.auth_hdr);
    assert_memory_equal(&hdr_seen[AT_LATE], verified, sizeof(*verified));
    /* To the jump */
    assert_int_equal(fake.hdr_given[AT_JUMP], 1);
    assert_memory_equal(&hdr_seen[AT_JUMP], verified, sizeof(*verified));
}

static void test_header_before_any_boot(void **state)
{
    /* The contract's type: read only */
    const struct bpak_header *(*get)(void) = boot_image_get_bpak_header;
    (void)state;

    assert_null(get());
}

/* A partition that is loaded, the policy's candidate */
static void test_header_bio_boot(void **state)
{
    struct fixture *f = *state;
    struct bpak_header verified;

    as_authenticated(sys_header(f, 0), &verified);
    assert_int_equal(boot_to_the_jump(NULL), JUMPED);

    assert_memory_equal(&auth_seen, sys_header(f, 0), sizeof(auth_seen));
    assert_header_given(&verified, true);
    /* Before the load there was none to give: the hook and the policy asked */
    assert_int_equal(fake.hdr_asked[AT_EARLY], 1);
    assert_int_equal(fake.hdr_asked[AT_NEXT], 1);
    /* Once in prepare, and it is there when the policy hears of the success */
    assert_int_equal(fake.hdr_given[AT_PREPARE], 1);
    assert_int_equal(fake.hdr_given[AT_REPORT], 1);
}

/* The partition the host names (boot part) */
static void test_header_bio_boot_override(void **state)
{
    struct fixture *f = *state;
    struct bpak_header verified;

    as_authenticated(sys_header(f, 1), &verified);
    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    assert_int_equal(boot_to_the_jump((unsigned char *)UU_SYS_B), JUMPED);

    assert_header_given(&verified, true);
}

/* In place: the header is pb's copy of the one at the partition's end */
static void test_header_in_place_boot(void **state)
{
    struct fixture *f = *state;
    struct bpak_header verified;

    as_authenticated(sys_header(f, 0), &verified);
    assert_int_equal(boot_to_the_jump(NULL), JUMPED);

    assert_int_equal(fake.in_place_calls, 1);
    assert_header_given(&verified, true);
    assert_true((const uint8_t *)fake.hdr_at[AT_LATE] < f->disk->data ||
                (const uint8_t *)fake.hdr_at[AT_LATE] >=
                    f->disk->data + DISK_BLOCKS * RAMDISK_BLOCK_SIZE);
}

/* The host's stream (boot bpak) */
static void test_header_cb_boot(void **state)
{
    uint8_t raw[sizeof(struct bpak_header)];
    struct bpak_header verified;
    (void)state;

    stream_header(raw, sizeof(raw));
    as_authenticated(raw, &verified);
    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    assert_int_equal(boot_to_the_jump((unsigned char *)UU_SYS_B), JUMPED);

    assert_header_given(&verified, true);
    /* None while the header itself was read */
    assert_int_equal(fake.hdr_asked[AT_READ], 1);
    assert_int_equal(fake.hdr_asked[AT_RESULT], 2);
}

/*
 * A boot that fails has no header when it has failed, whichever step failed,
 * and gave none on the way, except to a prepare that then failed. The boot
 * before it verified one, which is gone too. 'placement_rc': the placement
 * check of the source.
 */
static void header_gone_after_each_failure(int *placement_rc)
{
    int *step_rc[] = {
        &fake.auth_rc, placement_rc, &fake.load_rc, &fake.verify_payload_rc, &fake.prepare_rc,
    };

    for (size_t i = 0; i < sizeof(step_rc) / sizeof(step_rc[0]); i++) {
        bool in_prepare = (step_rc[i] == &fake.prepare_rc);

        assert_int_equal(boot_load(NULL), PB_OK);
        assert_non_null(boot_image_get_bpak_header());
        memset(fake.hdr_given, 0, sizeof(fake.hdr_given));

        *step_rc[i] = -PB_ERR_IO;
        assert_int_equal(boot_load(NULL), -PB_ERR_IO);
        *step_rc[i] = PB_OK;

        assert_null(boot_image_get_bpak_header());
        assert_no_unverified_header();
        /* The hash was right when prepare ran */
        assert_int_equal(fake.hdr_given[AT_PREPARE], in_prepare ? 1 : 0);
        assert_int_equal(fake.hdr_given[AT_REPORT], 0);
    }

    assert_int_equal(fake.hdr_asked[AT_LATE], 0);
}

static void test_header_bio_boot_errors(void **state)
{
    struct fixture *f = *state;

    header_gone_after_each_failure(&fake.verify_parts_rc);

    /* The header can't be read: what is left of the one before isn't given */
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_non_null(boot_image_get_bpak_header());
    f->disk->fail_reads_after = 0;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    f->disk->fail_reads_after = -1;
    assert_null(boot_image_get_bpak_header());

    /* Nor when the boot stops before any candidate */
    assert_int_equal(boot_load(NULL), PB_OK);
    fake.early_rc = -PB_ERR_ABORT;
    assert_int_equal(boot_load(NULL), -PB_ERR_ABORT);
    assert_null(boot_image_get_bpak_header());
    fake.early_rc = PB_OK;

    assert_int_equal(boot_load(NULL), PB_OK);
    fake.no_of_candidates = 0;
    assert_int_equal(boot_load(NULL), -PB_ERR_NO_ACTIVE_BOOT_PARTITION);
    assert_null(boot_image_get_bpak_header());
}

static void test_header_in_place_boot_errors(void **state)
{
    (void)state;

    header_gone_after_each_failure(&fake.in_place_rc);
    assert_int_equal(fake.verify_parts_calls, 0);
}

static void test_header_cb_boot_errors(void **state)
{
    (void)state;

    assert_int_equal(boot_set_source(BOOT_SOURCE_CB), PB_OK);
    boot_configure_load_cb(fake_read_cb, fake_result_cb);
    header_gone_after_each_failure(&fake.verify_parts_rc);

    assert_int_equal(boot_load(NULL), PB_OK);
    fake.read_cb_rc = -PB_ERR_IO;
    assert_int_equal(boot_load(NULL), -PB_ERR_IO);
    assert_null(boot_image_get_bpak_header());
    fake.read_cb_rc = PB_OK;

    /* No stream to read from */
    assert_int_equal(boot_load(NULL), PB_OK);
    boot_configure_load_cb(NULL, NULL);
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
    assert_null(boot_image_get_bpak_header());
}

/* A delivery's prepare is part of the attempt: it has the header, and when
 * it fails the header is gone */
static void test_header_delivery_prepare_error(void **state)
{
    (void)state;

    assert_int_equal(boot_init(&delivery_driver), PB_OK);
    fake.delivery_prepare_rc = -PB_ERR_BAD_META;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_META);

    assert_int_equal(fake.hdr_given[AT_PREPARE], 1);
    assert_int_equal(fake.hdr_given[AT_DELIVERY_PREPARE], 1);
    assert_int_equal(fake.hdr_given[AT_REPORT], 0);
    assert_null(boot_image_get_bpak_header());
}

/* Two candidates (boot.md 2): the first one's payload hash is wrong. Its
 * header is never handed out, late_boot() gets the second one's */
static void test_header_second_candidate(void **state)
{
    struct fixture *f = *state;
    struct bpak_header a;
    struct bpak_header b;

    as_authenticated(sys_header(f, 0), &a);
    as_authenticated(sys_header(f, 1), &b);
    assert_memory_not_equal(&a, &b, sizeof(a));

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;
    fake.verify_payload_fail_call = 1;

    assert_int_equal(boot_to_the_jump(NULL), JUMPED);

    assert_int_equal(fake.auth_calls, 2);
    assert_int_equal(fake.report_calls, 2);
    assert_memory_equal(fake.report_uu[0], UU_SYS_A, 16);
    assert_int_equal(fake.report_result[0], -PB_ERR_BAD_PAYLOAD);
    assert_int_equal(fake.report_result[1], PB_OK);
    assert_memory_equal(fake.prepare_uu, UU_SYS_B, 16);

    /* None while A failed and B was loaded, B's from its prepare on */
    assert_header_given(&b, true);
    assert_int_equal(fake.hdr_asked[AT_NEXT], 2);
    assert_int_equal(fake.hdr_asked[AT_PAYLOAD], 2);
    assert_int_equal(fake.hdr_given[AT_PREPARE], 1);
    assert_int_equal(fake.hdr_given[AT_REPORT], 1);
}

/* The first candidate verifies and fails in prepare: its header was there
 * in its prepare, and is gone while the second one is loaded */
static void test_header_second_candidate_after_prepare(void **state)
{
    struct fixture *f = *state;
    struct bpak_header b;

    as_authenticated(sys_header(f, 1), &b);
    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;
    fake.prepare_fail_uu = UU_SYS_A;

    assert_int_equal(boot_to_the_jump(NULL), JUMPED);

    assert_int_equal(fake.report_result[0], -PB_ERR_BAD_META);
    assert_int_equal(fake.report_result[1], PB_OK);
    assert_int_equal(fake.prepare_calls, 2);
    /* Each prepare had its own candidate's: see_header() compares */
    assert_int_equal(fake.hdr_given[AT_PREPARE], 2);
    assert_header_given(&b, true);
    assert_int_equal(fake.hdr_given[AT_REPORT], 1);
}

/* Every candidate failed: none */
static void test_header_every_candidate_fails(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(bio_clear_set_flags(f->sys_b, 0, BIO_FLAG_BOOTABLE), PB_OK);
    fake.candidates[1] = UU_SYS_B;
    fake.no_of_candidates = 2;
    fake.prepare_fail_uu = UU_SYS_A;
    fake.verify_payload_fail_call = 2;

    assert_int_equal(boot(NULL), -PB_ERR_BAD_META);
    assert_int_equal(fake.report_calls, 2);
    assert_int_equal(fake.hdr_given[AT_PREPARE], 1);
    assert_null(boot_image_get_bpak_header());
}

/*
 * A boot that is stopped after prepare is over (board.md 5.3: valid from
 * prepare to the jump): late_boot() had the header, command mode's commands
 * and the next boot's early_boot() get none.
 */
static void test_header_after_a_stopped_boot(void **state)
{
    (void)state;

    /* late_boot() stops it */
    fake.late_rc = -PB_ERR_ABORT;
    assert_int_equal(boot(NULL), -PB_ERR_ABORT);
    assert_int_equal(fake.hdr_given[AT_LATE], 1);
    assert_null(boot_image_get_bpak_header());

    /* or fails */
    fake.late_rc = -PB_ERR_IO;
    assert_int_equal(boot(NULL), -PB_ERR_IO);
    assert_int_equal(fake.hdr_given[AT_LATE], 2);
    assert_null(boot_image_get_bpak_header());
    fake.late_rc = PB_OK;

    /* The lockdown fails */
    fake_lockdown_rc = -PB_ERR_IO;
    assert_int_equal(boot(NULL), -PB_ERR_IO);
    assert_int_equal(fake.hdr_given[AT_LATE], 3);
    assert_null(boot_image_get_bpak_header());
    fake_lockdown_rc = PB_OK;

    /* The jump comes back: the delivery had it, then the boot has failed */
    assert_int_equal(boot_init(&delivery_driver), PB_OK);
    assert_int_equal(boot(NULL), -PB_ERR);
    assert_int_equal(fake.hdr_given[AT_DELIVERY_PREPARE], 1);
    assert_int_equal(fake.hdr_given[AT_JUMP], 1);
    assert_null(boot_image_get_bpak_header());

    assert_int_equal(fake.jump_calls, 0);
}

/* "This boot": what the boot before verified isn't the next boot's, also
 * when that one was never stopped */
static void test_header_is_this_boots(void **state)
{
    (void)state;

    /* Loaded, verified and prepared, as command mode has it before the
     * disconnect: there, and the same again on the next call */
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_non_null(boot_image_get_bpak_header());
    assert_ptr_equal(boot_image_get_bpak_header(), fake.auth_hdr);
    assert_memory_equal(boot_image_get_bpak_header(), &auth_left, sizeof(auth_left));

    /* The next boot's early_boot() and its load get none of it */
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.hdr_asked[AT_EARLY], 2);
    assert_no_unverified_header();

    /* After a jump, were it to come back */
    assert_int_equal(boot_to_the_jump(NULL), JUMPED);
    assert_non_null(boot_image_get_bpak_header());
    fake.early_rc = -PB_ERR_ABORT;
    assert_int_equal(boot(NULL), -PB_ERR_ABORT);
    assert_int_equal(fake.hdr_asked[AT_EARLY], 4);
    assert_int_equal(fake.hdr_given[AT_EARLY], 0);
    assert_null(boot_image_get_bpak_header());

    /* And boot_init() starts over */
    fake.early_rc = PB_OK;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_non_null(boot_image_get_bpak_header());
    assert_int_equal(boot_init(&driver), PB_OK);
    assert_null(boot_image_get_bpak_header());
}

static void test_invalid_default_source(void **state)
{
    static const struct boot_driver bad_source = {
        .default_boot_source = BOOT_SOURCE_INVALID,
        .handover = &fake_handover,
    };
    (void)state;

    assert_int_equal(boot_init(&bad_source), PB_OK);
    assert_int_equal(boot_load(NULL), -PB_ERR_PARAM);
}

static void test_handover_removed_after_init(void **state)
{
    static struct boot_driver mutable_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .handover = &fake_handover,
    };
    (void)state;

    assert_int_equal(boot_init(&mutable_driver), PB_OK);
    mutable_driver.handover = NULL;
    assert_int_equal(boot_load(NULL), -PB_ERR_NOT_SUPPORTED);
    mutable_driver.handover = &fake_handover;
}

/* A board with a delivery (boot.md 5.1): it prepares after the handover,
 * with the handover's entry, and it jumps, not the core */
static void test_delivery(void **state)
{
    (void)state;

    assert_int_equal(boot_init(&delivery_driver), PB_OK);
    boot_clear_set_flags(0, BOOT_FLAG_VERBOSE);
    /* The fake delivery's jump comes back, which is a failed boot: an error,
     * and the flags are cleared */
    assert_int_equal(boot(NULL), -PB_ERR);
    assert_int_equal(fake.late_flags, BOOT_FLAG_VERBOSE);
    assert_int_equal(boot_get_flags(), 0);

    assert_int_equal(fake.prepare_calls, 1);
    assert_int_equal(fake.delivery_prepare_calls, 1);
    assert_int_equal(fake.delivery_prepare_after, 1);
    assert_ptr_equal(fake.delivery_prepare_hdr, fake.prepare_hdr);
    assert_true(fake.delivery_prepare_next.entry == NEXT_ENTRY);
    assert_true(fake.delivery_prepare_next.arg[0] == NEXT_ARG0);
    assert_true(fake.delivery_prepare_next.arg[3] == NEXT_ARG3);
    assert_true(fake.delivery_prepare_next.dt == NEXT_DT);

    /* After late_boot and the lockdown, as the direct jump, and after the
     * watchdog's kick: what the delivery does before BL31 runs is the next
     * stage's time */
    assert_int_equal(fake.late_calls, 1);
    assert_int_equal(fake_lockdown_calls, 1);
    assert_int_equal(fake.delivery_jump_calls, 1);
    assert_int_equal(fake.kicks_at_late, 0);
    assert_int_equal(fake_lockdown_wdog_kicks, 0);
    assert_int_equal(fake.kicks_at_jump, 1);
    assert_int_equal(fake_wdog_kicks, 1);
    assert_true(fake.delivery_jump_next.entry == NEXT_ENTRY);
    assert_true(fake.delivery_jump_next.arg[0] == NEXT_ARG0);
    assert_true(fake.delivery_jump_next.arg[1] == NEXT_ARG1);
    assert_true(fake.delivery_jump_next.arg[2] == NEXT_ARG2);
    assert_true(fake.delivery_jump_next.arg[3] == NEXT_ARG3);
    assert_true(fake.delivery_jump_next.dt == NEXT_DT);

    /* The cache and the jump are the delivery's: the core does neither */
    assert_int_equal(fake.jump_calls, 0);
    assert_int_equal(fake.cleaned_calls, 0);
    assert_int_equal(fake.mmu_disabled, 0);
}

static void test_delivery_errors(void **state)
{
    (void)state;

    assert_int_equal(boot_init(&delivery_driver), PB_OK);

    /* What the delivery needs isn't in the image: the boot fails in prepare */
    boot_clear_set_flags(0, BOOT_FLAG_CMD);
    fake.delivery_prepare_rc = -PB_ERR_BAD_META;
    assert_int_equal(boot(NULL), -PB_ERR_BAD_META);
    assert_int_equal(fake.late_calls, 0);
    assert_int_equal(fake.delivery_jump_calls, 0);
    assert_int_equal(boot_get_flags(), 0);
    fake.delivery_prepare_rc = PB_OK;

    /* A failed handover: the delivery isn't asked */
    fake.delivery_prepare_calls = 0;
    fake.prepare_rc = -PB_ERR_BAD_META;
    assert_int_equal(boot(NULL), -PB_ERR_BAD_META);
    assert_int_equal(fake.delivery_prepare_calls, 0);
    assert_int_equal(fake.delivery_jump_calls, 0);
    fake.prepare_rc = PB_OK;

    /* No jump without the lockdown, with a delivery too */
    fake_lockdown_rc = -PB_ERR_IO;
    assert_int_equal(boot(NULL), -PB_ERR_IO);
    assert_int_equal(fake.delivery_jump_calls, 0);
    assert_int_equal(fake_wdog_kicks, 0);
}

/* Every prepare starts from an entry that is all zero: nothing of an earlier
 * image's handover is left in it */
static void test_prepare_starts_from_zeros(void **state)
{
    (void)state;

    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(boot_load(NULL), PB_OK);
    fake.prepare_rc = -PB_ERR_BAD_META;
    assert_int_equal(boot_load(NULL), -PB_ERR_BAD_META);
    fake.prepare_rc = PB_OK;
    assert_int_equal(boot_load(NULL), PB_OK);
    assert_int_equal(fake.prepare_calls, 4);
    assert_true(fake.prepare_got_zeros);
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
        BOOT_TEST(test_bio_boot_smaller_than_a_header),
        BOOT_TEST(test_bio_boot_needs_a_policy),
        BOOT_TEST(test_policy_next_candidate),
        BOOT_TEST(test_policy_next_candidate_after_prepare),
        BOOT_TEST(test_policy_every_candidate_fails),
        BOOT_TEST(test_policy_next_fails_after_a_candidate),
        BOOT_TEST(test_info_is_collected_once_before_the_candidates),
        BOOT_TEST(test_info_key_that_cannot_be_read),
        BOOT_TEST(test_info_without_an_slc),
        BOOT_TEST(test_info_that_cannot_be_read_stops_the_boot),
        BOOT_TEST(test_info_of_the_candidate),
        BOOT_TEST(test_info_of_the_hosts_stream),
        BOOT_TEST(test_info_without_a_system_name),
        BOOT_TEST(test_mode_of_the_policys_candidates),
        BOOT_TEST(test_mode_is_normal_after_a_failed_fallback),
        BOOT_TEST(test_precondition_dram_filter),
        BOOT_TEST(test_precondition_in_a_boot_command_mode_started),
        BOOT_TEST(test_precondition_in_every_life_cycle_state),
        BOOT_TEST(test_precondition_watchdog),
        BOOT_TEST(test_precondition_crypto_engine),
        BOOT_TEST(test_precondition_rom_log),
        BOOT_TEST(test_early_boot_abort),
        BOOT_TEST(test_late_boot_error),
        BOOT_TEST(test_lockdown_error),
        MAPPED_TEST(test_in_place_boot),
        MAPPED_TEST(test_in_place_boot_override),
        MAPPED_TEST(test_in_place_other_partition_is_loaded),
        MAPPED_TEST(test_in_place_not_bootable),
        MAPPED_TEST(test_in_place_boot_errors),
        MAPPED_TEST(test_in_place_is_told_to_the_handover),
        MAPPED_TEST(test_in_place_stays_in_place),
        MAPPED_TEST(test_in_place_smaller_than_a_header),
        MAPPED_TEST(test_in_place_cb_boot_is_loaded),
        /* After the mapped cases: what they left mapped is gone */
        BOOT_TEST(test_bio_boot_after_in_place),
        BOOT_TEST(test_cb_boot),
        BOOT_TEST(test_cb_boot_errors),
        BOOT_TEST(test_header_before_any_boot),
        BOOT_TEST(test_header_bio_boot),
        BOOT_TEST(test_header_bio_boot_override),
        MAPPED_TEST(test_header_in_place_boot),
        BOOT_TEST(test_header_cb_boot),
        BOOT_TEST(test_header_bio_boot_errors),
        MAPPED_TEST(test_header_in_place_boot_errors),
        BOOT_TEST(test_header_cb_boot_errors),
        BOOT_TEST(test_header_delivery_prepare_error),
        BOOT_TEST(test_header_second_candidate),
        BOOT_TEST(test_header_second_candidate_after_prepare),
        BOOT_TEST(test_header_every_candidate_fails),
        BOOT_TEST(test_header_after_a_stopped_boot),
        BOOT_TEST(test_header_is_this_boots),
        BOOT_TEST(test_invalid_default_source),
        BOOT_TEST(test_handover_removed_after_init),
        BOOT_TEST(test_delivery),
        BOOT_TEST(test_delivery_errors),
        BOOT_TEST(test_prepare_starts_from_zeros),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
