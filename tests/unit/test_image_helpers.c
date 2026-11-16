/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * bpak image helpers (src/boot/image_helpers.c) with fake hash and DSA
 * providers and a fake root of trust, so every step can fail on demand.
 * Real signatures are covered by the integration tests (test_boot.py).
 *
 * 'pb' and 'buffers' are set with --defsym, like for pb-host, see
 * tests/unit/CMakeLists.txt. Built twice: as pb-host is, and with a Firmware
 * Handoff region and a platform's region for bl_params
 * (test_image_helpers_handover), which no image data region may overlap
 * either.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/image_helpers.h>
#include <boot/image_regions.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/plat.h"

/* Must match the --defsym values in CMakeLists.txt */
#define PB_START      0x40000000
#define PB_END        0x40100000
#define BUFFERS_START 0x40100000
#define BUFFERS_END   0x40120000

/* The board's image data regions: two, with a gap that the handover's
 * regions of the second build are in (CMakeLists.txt) */
#define REGION_A      0x48000000
#define REGION_A_SIZE 0x08000000
#define REGION_B      0x58000000
#define REGION_B_SIZE 0x02000000
#define FREE_RAM      0x49000000

#define ID_KERNEL     0xec103b08
#define ID_DTB        0x56f91b86

#define FAKE_SIG_OK   0x5a

static const struct boot_image_region image_regions[] = {
    BOOT_IMAGE_REGION(REGION_A, REGION_A_SIZE),
    BOOT_IMAGE_REGION(REGION_B, REGION_B_SIZE),
    BOOT_IMAGE_REGION_END,
};

static struct {
    int key_status_rc;
    int get_key_rc;

    int init_rc;
    int update_rc;
    int final_rc;
    hash_t alg;
    size_t hashed_bytes;
    uint32_t sum;
    /* The updates since the last init, and the watchdog's kicks at each */
    size_t updates;
    unsigned int kicks_at_update[8];

    int verify_rc;
    bool verify_ok;
    hash_t verify_md_alg;
    size_t verify_md_length;
} fake;

int rot_read_key_status(key_id_t id)
{
    (void)id;
    return fake.key_status_rc;
}

int rot_get_dsa_key(key_id_t id, dsa_t *dsa_kind, const uint8_t **der_data, size_t *der_data_length)
{
    static const uint8_t der[] = { 0x30 };
    (void)id;

    *dsa_kind = DSA_EC_SECP256r1;
    *der_data = der;
    *der_data_length = sizeof(der);
    return fake.get_key_rc;
}

static int fake_init(hash_t alg)
{
    fake.alg = alg;
    fake.hashed_bytes = 0;
    fake.sum = 0;
    fake.updates = 0;
    return fake.init_rc;
}

static int fake_update(const void *buf, size_t length)
{
    const uint8_t *p = buf;

    if (fake.updates < ARRAY_SIZE(fake.kicks_at_update))
        fake.kicks_at_update[fake.updates] = fake_wdog_kicks;
    fake.updates++;

    if (fake.update_rc != PB_OK)
        return fake.update_rc;

    for (size_t i = 0; i < length; i++)
        fake.sum = fake.sum * 31 + p[i];
    fake.hashed_bytes += length;
    return PB_OK;
}

/* The digest is the running sum, repeated */
static void fake_digest(uint8_t *out, size_t length, uint32_t sum)
{
    for (size_t i = 0; i < length; i++)
        out[i] = (uint8_t)(sum >> (8 * (i % 4)));
}

static int fake_final(uint8_t *digest_out, size_t length)
{
    if (fake.final_rc != PB_OK)
        return fake.final_rc;
    fake_digest(digest_out, length, fake.sum);
    return PB_OK;
}

static const struct hash_ops fake_hash_ops = {
    .name = "fake-hash",
    .alg_bits = HASH_SHA256 | HASH_SHA384 | HASH_SHA512,
    .init = fake_init,
    .update = fake_update,
    .final = fake_final,
};

static int fake_verify(const uint8_t *der_signature,
                       size_t signature_length,
                       const uint8_t *der_key,
                       size_t key_length,
                       hash_t md_alg,
                       uint8_t *md,
                       size_t md_length,
                       bool *verified)
{
    (void)der_key;
    (void)key_length;
    (void)md;
    fake.verify_md_alg = md_alg;
    fake.verify_md_length = md_length;
    *verified = fake.verify_ok && (signature_length == 2) && (der_signature[0] == FAKE_SIG_OK);
    return fake.verify_rc;
}

static const struct dsa_ops fake_dsa_ops = {
    .name = "fake-dsa",
    .alg_bits = DSA_EC_SECP256r1,
    .verify = fake_verify,
};

static struct bpak_header *hdr;

/* Two parts, kernel and dtb, loaded at 'kernel_addr' and 'dtb_addr' */
static void make_header(uint8_t hash_kind,
                        uint64_t kernel_addr,
                        size_t kernel_size,
                        uint64_t dtb_addr,
                        size_t dtb_size)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *meta;

    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    hdr->hash_kind = hash_kind;

    assert_int_equal(bpak_add_part(hdr, ID_KERNEL, &part), BPAK_OK);
    part->size = kernel_size;
    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_KERNEL, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = kernel_addr;

    if (dtb_size) {
        assert_int_equal(bpak_add_part(hdr, ID_DTB, &part), BPAK_OK);
        part->size = dtb_size;
        assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, 8, &meta), BPAK_OK);
        *bpak_get_meta_ptr(hdr, meta, uint64_t) = dtb_addr;
    }

    hdr->signature[0] = FAKE_SIG_OK;
    hdr->signature_sz = 2;
}

static int group_setup(void **state)
{
    (void)state;

    if (hash_add_ops(&fake_hash_ops) != PB_OK)
        return -1;
    if (dsa_add_ops(&fake_dsa_ops) != PB_OK)
        return -1;
    hdr = malloc(sizeof(*hdr));
    return hdr ? 0 : -1;
}

static int group_teardown(void **state)
{
    (void)state;
    free(hdr);
    return 0;
}

static int setup(void **state)
{
    (void)state;
    memset(&fake, 0, sizeof(fake));
    fake.verify_ok = true;
    fake_wdog_kicks = 0;
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, FREE_RAM + 0x100000, 512);
    return boot_image_set_regions(image_regions) == PB_OK ? 0 : -1;
}

static void test_auth_header(void **state)
{
    static const struct {
        uint8_t bpak;
        hash_t pb;
        size_t digest_length;
    } kinds[] = {
        { BPAK_HASH_SHA256, HASH_SHA256, 32 },
        { BPAK_HASH_SHA384, HASH_SHA384, 48 },
        { BPAK_HASH_SHA512, HASH_SHA512, 64 },
    };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(kinds); i++) {
        make_header(kinds[i].bpak, FREE_RAM, 1024, 0, 0);
        assert_int_equal(boot_image_auth_header(hdr), PB_OK);
        assert_int_equal(fake.alg, kinds[i].pb);
        assert_int_equal(fake.verify_md_alg, kinds[i].pb);
        /* Only the digest, not the whole buffer (F-058) */
        assert_int_equal(fake.verify_md_length, kinds[i].digest_length);
        /* The whole header is hashed, with the signature zeroed */
        assert_int_equal(fake.hashed_bytes, sizeof(*hdr));
        assert_int_equal(hdr->signature_sz, 0);
    }
}

static void test_auth_header_errors(void **state)
{
    (void)state;

    hdr->magic = 0;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_BAD_HEADER);
    hdr->magic = BPAK_HEADER_MAGIC;

    fake.key_status_rc = -PB_ERR_KEY_REVOKED;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_KEY_REVOKED);
    fake.key_status_rc = PB_OK;

    fake.get_key_rc = -PB_ERR_KEY_NOT_FOUND;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_KEY_NOT_FOUND);
    fake.get_key_rc = PB_OK;

    hdr->hash_kind = 99;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_UNKNOWN_HASH);
    hdr->hash_kind = BPAK_HASH_SHA256;

    fake.init_rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_NOT_SUPPORTED);
    fake.init_rc = PB_OK;

    hdr->signature_sz = 0;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_BUF_TOO_SMALL);
    hdr->signature_sz = 2;

    fake.update_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_IO);
    fake.update_rc = PB_OK;
    hdr->signature_sz = 2; /* Cleared by the previous attempt */

    fake.final_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_IO);
    fake.final_rc = PB_OK;
    hdr->signature_sz = 2;

    fake.verify_rc = -PB_ERR_SIGNATURE;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_AUTHENTICATION_FAILED);
    fake.verify_rc = PB_OK;
    hdr->signature_sz = 2;

    fake.verify_ok = false;
    assert_int_equal(boot_image_auth_header(hdr), -PB_ERR_AUTHENTICATION_FAILED);
}

static void test_verify_parts(void **state)
{
    (void)state;

    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);

    hdr->magic = 0;
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_BAD_HEADER);
}

static void test_verify_parts_needs_load_addr(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, &meta), BPAK_OK);
    meta->part_id_ref = ID_KERNEL + 1;
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_BAD_META);
}

/* A part lies inside one region, wherever in it (boot.md 4.3) */
static void test_verify_parts_inside_a_region(void **state)
{
    (void)state;

    /* One part in each region */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, REGION_B + 0x1000, 1024);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);

    /* From a region's first byte, and to its last */
    make_header(BPAK_HASH_SHA256, REGION_A, 1024, REGION_A + REGION_A_SIZE - 1024, 1024);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);

    /* One part that is the whole region */
    make_header(BPAK_HASH_SHA256, REGION_B, REGION_B_SIZE, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);

    /* A part without bytes is placed like any other */
    make_header(BPAK_HASH_SHA256, REGION_B + REGION_B_SIZE, 0, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);
    make_header(BPAK_HASH_SHA256, REGION_B + REGION_B_SIZE + 1, 0, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
}

/* The regions are the only memory a part is loaded into: pb's own memory is
 * outside them like any other */
static void test_verify_parts_outside_the_regions(void **state)
{
    static const struct {
        uint64_t addr;
        size_t size;
    } parts[] = {
        { PB_START + 0x1000, 1024 }, /* pb's own memory */
        { PB_END - 512, 1024 },
        { BUFFERS_START + 0x1000, 1024 },
        { BUFFERS_END, 1024 }, /* RAM that is pb's no more, and in no region */
        { 0, 1024 },
        { REGION_A - 0x200, 1024 }, /* Ends inside a region */
        { REGION_A - 1024, 1024 }, /* Ends where it starts */
        { REGION_A + REGION_A_SIZE - 512, 1024 }, /* Starts inside */
        { REGION_A + REGION_A_SIZE, 1024 }, /* The gap between the two */
        { REGION_A, REGION_A_SIZE + 512 },
        { REGION_A + 1, REGION_A_SIZE }, /* By one byte */
        { REGION_B + REGION_B_SIZE, 1024 }, /* Above them */
        /* Above 4 GiB: not narrowed to an address in a region */
        { 0x100000000ull + FREE_RAM, 1024 },
    };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(parts); i++) {
        /* The second part is outside */
        make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, parts[i].addr, parts[i].size);
        assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

        /* And the first */
        make_header(BPAK_HASH_SHA256, parts[i].addr, parts[i].size, FREE_RAM, 1024);
        assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    }
}

/* Inside one region: not across two, also when they are next to each other
 * and every byte of the part is in one of them */
static void test_verify_parts_across_two_regions(void **state)
{
    static const struct boot_image_region adjacent[] = {
        BOOT_IMAGE_REGION(FREE_RAM, 0x1000),
        BOOT_IMAGE_REGION(FREE_RAM + 0x1000, 0x1000),
        BOOT_IMAGE_REGION_END,
    };
    (void)state;

    assert_int_equal(boot_image_set_regions(adjacent), PB_OK);

    make_header(BPAK_HASH_SHA256, FREE_RAM, 0x1000, FREE_RAM + 0x1000, 0x1000);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);

    make_header(BPAK_HASH_SHA256, FREE_RAM + 0x800, 0x1000, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    make_header(BPAK_HASH_SHA256, FREE_RAM, 0x2000, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    /* One byte of it in the second */
    make_header(BPAK_HASH_SHA256, FREE_RAM + 0xe01, 512, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
}

/* An address range that wraps is in no region, wherever its end lands */
static void test_verify_parts_wraps(void **state)
{
    /* The last 64 KiB of the address space, which a 64-bit arch can have */
    static const struct boot_image_region top[] = {
        BOOT_IMAGE_REGION(REGION_A, REGION_A_SIZE),
        BOOT_IMAGE_REGION(UINT64_MAX - 0xffff, 0x10000),
        BOOT_IMAGE_REGION_END,
    };
    (void)state;

    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, UINT64_MAX - 511, 1024);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    /* Wraps into a region: its end is FREE_RAM + 0x1000 */
    make_header(BPAK_HASH_SHA256, UINT64_MAX - 0xfff, (size_t)FREE_RAM + 0x2000, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    assert_int_equal(boot_image_set_regions(top), PB_OK);

    /* To the last byte there is */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, UINT64_MAX - 1023, 1024);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);

    /* One byte further wraps, out of the region it starts in */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, UINT64_MAX - 1022, 1024);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    make_header(BPAK_HASH_SHA256, UINT64_MAX - 0xfff, (size_t)FREE_RAM + 0x2000, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    /* Two parts over each other at the last byte there is: the end of both
     * is 0, their last bytes are compared */
    make_header(BPAK_HASH_SHA256, UINT64_MAX - 1023, 1024, UINT64_MAX - 511, 512);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    make_header(BPAK_HASH_SHA256, UINT64_MAX - 1023, 512, UINT64_MAX - 511, 512);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);
}

/* A part overlaps no other part */
static void test_verify_parts_over_each_other(void **state)
{
    (void)state;

    /* The second starts inside the first */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 0x2000, FREE_RAM + 0x1000, 0x2000);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    /* The first starts inside the second */
    make_header(BPAK_HASH_SHA256, FREE_RAM + 0x1000, 0x2000, FREE_RAM, 0x2000);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    /* By one byte */
    make_header(BPAK_HASH_SHA256, FREE_RAM + 1, 0x1000, FREE_RAM + 0x1000, 0x1000);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    /* The same address */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, FREE_RAM, 1024);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    /* One inside the other */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 0x4000, FREE_RAM + 0x1000, 512);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    /* Next to each other, in either order */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 0x1000, FREE_RAM + 0x1000, 0x1000);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);
    make_header(BPAK_HASH_SHA256, FREE_RAM + 0x1000, 0x1000, FREE_RAM, 0x1000);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);
}

/* A board without a list loads nothing (board.md 2.9). The in place source
 * has its own allowed memory */
static void test_verify_parts_without_regions(void **state)
{
    (void)state;

    assert_int_equal(boot_image_set_regions(NULL), PB_OK);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    assert_false(boot_image_in_regions(FREE_RAM, 1));
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FREE_RAM, 0x200000), PB_OK);
}

/* The room a handover grows a part by is asked for with the part */
static void test_in_regions(void **state)
{
    (void)state;

    assert_true(boot_image_in_regions(REGION_A, REGION_A_SIZE));
    assert_true(boot_image_in_regions(REGION_B + REGION_B_SIZE - 0x1000, 0x1000));
    assert_false(boot_image_in_regions(REGION_B + REGION_B_SIZE - 0x1000, 0x1001));
    assert_false(boot_image_in_regions(REGION_A - 1, 2));
    assert_false(boot_image_in_regions(PB_START, 1));
    assert_false(boot_image_in_regions(REGION_A, UINT64_MAX));
    assert_false(boot_image_in_regions(UINT64_MAX, 2));
}

/* The check of the board's list (board.md 3): the list that passes is the
 * one in use, and one that fails leaves none */
static void assert_regions_fail(uint64_t base, uint64_t size)
{
    const struct boot_image_region alone[] = {
        BOOT_IMAGE_REGION(base, size),
        BOOT_IMAGE_REGION_END,
    };
    const struct boot_image_region last[] = {
        BOOT_IMAGE_REGION(REGION_A, REGION_A_SIZE),
        BOOT_IMAGE_REGION(base, size),
        BOOT_IMAGE_REGION_END,
    };

    assert_int_equal(boot_image_set_regions(image_regions), PB_OK);
    assert_int_equal(boot_image_set_regions(alone), -PB_ERR_PARAM);
    /* Nothing is loaded, not even where the list before it allowed */
    assert_false(boot_image_in_regions(FREE_RAM, 1024));
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, 0, 0);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);

    /* After a region that passes */
    assert_int_equal(boot_image_set_regions(last), -PB_ERR_PARAM);
    assert_false(boot_image_in_regions(FREE_RAM, 1024));
}

static void assert_regions_pass(uint64_t base, uint64_t size)
{
    const struct boot_image_region alone[] = {
        BOOT_IMAGE_REGION(base, size),
        BOOT_IMAGE_REGION_END,
    };

    assert_int_equal(boot_image_set_regions(alone), PB_OK);
    assert_true(boot_image_in_regions(base, size));
}

static void test_regions_overlap_pb(void **state)
{
    (void)state;

    /* The board's whole RAM, with pb at its start */
    assert_regions_fail(PB_START, 0x20000000);
    /* Inside pb, over its start, over its end */
    assert_regions_fail(PB_START + 0x1000, 0x1000);
    assert_regions_fail(PB_START - 0x1000, 0x1001);
    assert_regions_fail(PB_END - 1, 0x1000);
    /* All of pb and more */
    assert_regions_fail(PB_START - 0x1000, 0x40000000);

    /* 'buffers' */
    assert_regions_fail(BUFFERS_START + 0x1000, 0x1000);
    assert_regions_fail(BUFFERS_END - 1, 0x08000000);

    /* Up to pb's first byte, and from the first after 'buffers' */
    assert_regions_pass(PB_START - 0x1000, 0x1000);
    assert_regions_pass(BUFFERS_END, 0x1000);
}

static void test_regions_empty_or_wrapping(void **state)
{
    static const struct boot_image_region none[] = { BOOT_IMAGE_REGION_END };
    (void)state;

    assert_regions_fail(FREE_RAM, 0);
    assert_regions_fail(0, 0);
    assert_regions_fail(UINT64_MAX - 0xfff, 0x1001);
    assert_regions_fail(FREE_RAM, UINT64_MAX);
    assert_regions_fail(UINT64_MAX, 2);

    /* Memory at address 0, and to the last byte there is */
    assert_regions_pass(0, 0x20000);
    assert_regions_pass(UINT64_MAX - 0xfff, 0x1000);

    /* A list without regions passes, and loads nothing */
    assert_int_equal(boot_image_set_regions(none), PB_OK);
    assert_false(boot_image_in_regions(FREE_RAM, 1024));
}

/* What a handover writes outside the parts (boot.md 4.3), in the build that
 * has it */
static void test_regions_overlap_handover(void **state)
{
    (void)state;

#ifdef CONFIG_BOOT_FH
    assert_regions_fail(CONFIG_BOOT_FH_REGION_BASE, CONFIG_BOOT_FH_REGION_SIZE);
    assert_regions_fail(CONFIG_BOOT_FH_REGION_BASE - 0x1000, 0x1001);
    assert_regions_fail(CONFIG_BOOT_FH_REGION_BASE + CONFIG_BOOT_FH_REGION_SIZE - 1, 0x1000);
    assert_regions_fail(REGION_A, REGION_B - REGION_A);
    assert_regions_pass(CONFIG_BOOT_FH_REGION_BASE - 0x1000, 0x1000);
    assert_regions_pass(CONFIG_BOOT_FH_REGION_BASE + CONFIG_BOOT_FH_REGION_SIZE, 0x1000);
#endif

#ifdef PLAT_BL_PARAMS_BASE
    assert_regions_fail(PLAT_BL_PARAMS_BASE, 0x1000);
    assert_regions_fail(PLAT_BL_PARAMS_BASE - 0x1000, 0x1001);
    assert_regions_fail(PLAT_BL_PARAMS_BASE + PLAT_BL_PARAMS_SIZE - 1, 0x1000);
    assert_regions_pass(PLAT_BL_PARAMS_BASE - 0x1000, 0x1000);
    assert_regions_pass(PLAT_BL_PARAMS_BASE + PLAT_BL_PARAMS_SIZE, 0x1000);
#endif

#if !defined(CONFIG_BOOT_FH) && !defined(PLAT_BL_PARAMS_BASE)
    /* Neither: the gap between the suite's regions is the board's to list */
    assert_regions_pass(REGION_A, REGION_B - REGION_A);
#endif
}

/* A partition that is booted in place: its mapped range, without the header */
#define FLASH_BASE 0x60080000
#define FLASH_SIZE 0x000ff000

static void test_verify_parts_in_place(void **state)
{
    (void)state;

    make_header(BPAK_HASH_SHA256, FLASH_BASE, 1024, FLASH_BASE + 0x1000, 512);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE), PB_OK);

    /* The last part ends where the range ends */
    make_header(BPAK_HASH_SHA256, FLASH_BASE, 1024, FLASH_BASE + FLASH_SIZE - 512, 512);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE), PB_OK);

    /* One part that is the whole range */
    make_header(BPAK_HASH_SHA256, FLASH_BASE, FLASH_SIZE, 0, 0);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE), PB_OK);

    /* Flash that is mapped at 0 */
    make_header(BPAK_HASH_SHA256, 0, 1024, 0x2000 - 512, 512);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, 0, 0x2000), PB_OK);

    /* An image without parts has none outside */
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE), PB_OK);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, 0, 0), PB_OK);
}

/* Nothing is loaded, so the image data regions aren't what a part must be in */
static void test_verify_parts_in_place_ignores_pb_memory(void **state)
{
    (void)state;

    make_header(BPAK_HASH_SHA256, PB_START + 0x1000, 1024, PB_START + 0xe1000, 1024);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, PB_START, 0x200000), PB_OK);
}

static void test_verify_parts_in_place_outside(void **state)
{
    static const struct {
        uint64_t addr;
        size_t size;
    } outside[] = {
        /* Starts one byte below the range */
        { FLASH_BASE - 1, 512 },
        /* All of it below */
        { FLASH_BASE - 0x1000, 512 },
        /* Ends one byte past the range */
        { FLASH_BASE + FLASH_SIZE - 511, 512 },
        /* Starts where the range ends */
        { FLASH_BASE + FLASH_SIZE, 512 },
        /* Larger than the range */
        { FLASH_BASE, FLASH_SIZE + 512 },
        { FLASH_BASE + 512, (size_t)1 << 62 },
        /* The address and the size wrap in 64 bits: to 0, and to inside the range */
        { UINT64_MAX - 511, 512 },
        { UINT64_MAX - 100, 512 },
        { UINT64_MAX - 511, 512 + FLASH_BASE + 4096 },
        /* An address a 32-bit arch can't hold, its low half inside the range */
        { 0x100000000ULL + FLASH_BASE, 512 },
    };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(outside); i++) {
        /* As the second part, the first is inside */
        make_header(BPAK_HASH_SHA256, FLASH_BASE, 1024, outside[i].addr, outside[i].size);
        assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE),
                         -PB_ERR_MEM);

        /* And as the first */
        make_header(BPAK_HASH_SHA256, outside[i].addr, outside[i].size, FLASH_BASE, 1024);
        assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE),
                         -PB_ERR_MEM);
    }

    /* Mapped at 0: past the end, and wrapped around to the start */
    make_header(BPAK_HASH_SHA256, 0, 1024, 0x2000 - 511, 512);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, 0, 0x2000), -PB_ERR_MEM);
    make_header(BPAK_HASH_SHA256, 0, 1024, UINT64_MAX - 255, 512);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, 0, 0x2000), -PB_ERR_MEM);

    /* A range at the top of the address space */
    make_header(BPAK_HASH_SHA256, UINTPTR_MAX - 1023, 1024, 0, 0);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, UINTPTR_MAX - 1023, 1024), PB_OK);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, UINTPTR_MAX - 1023, 1023), -PB_ERR_MEM);

    /* A partition that only holds the header has room for no part */
    make_header(BPAK_HASH_SHA256, FLASH_BASE, 512, 0, 0);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, 0), -PB_ERR_MEM);
}

static void test_verify_parts_in_place_errors(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    make_header(BPAK_HASH_SHA256, FLASH_BASE, 1024, FLASH_BASE + 0x1000, 512);

    /* The dtb has no load address */
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, &meta), BPAK_OK);
    meta->part_id_ref = ID_KERNEL + 1;
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE),
                     -PB_ERR_BAD_META);

    hdr->magic = 0;
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE),
                     -PB_ERR_BAD_HEADER);
}

/* Fake storage: the image payload, read through read_f */
static uint8_t payload[3 * 1024];
static uint8_t load_area[3 * 1024];
static int reads;
static int read_rc;
static int fail_read_at;
static int results;
static int last_result;
/* The watchdog's kicks when each read was asked for */
static unsigned int kicks_at_read[8];

static int read_f(int64_t offset, size_t length, void *buf)
{
    if ((size_t)reads < ARRAY_SIZE(kicks_at_read))
        kicks_at_read[reads] = fake_wdog_kicks;
    reads++;
    if (reads == fail_read_at)
        return read_rc;
    memcpy(buf, &payload[offset], length);
    return PB_OK;
}

static int result_f(int rc)
{
    results++;
    last_result = rc;
    return rc;
}

/* Like command mode: the result is sent, whatever it is */
static int result_sent_f(int rc)
{
    results++;
    last_result = rc;
    return PB_OK;
}

/* The transport is gone: the result can't be sent */
static int result_fails_f(int rc)
{
    results++;
    last_result = rc;
    return -PB_ERR_IO;
}

static void load_setup(void)
{
    for (size_t i = 0; i < sizeof(payload); i++)
        payload[i] = (uint8_t)(i * 7);
    memset(load_area, 0, sizeof(load_area));
    reads = 0;
    read_rc = PB_OK;
    fail_read_at = 0;
    results = 0;
    last_result = 1;
    memset(kicks_at_read, 0xff, sizeof(kicks_at_read));
    fake_wdog_kicks = 0;
    /* Kernel first, then the dtb, back to back in the load area */
    make_header(BPAK_HASH_SHA256, (uintptr_t)load_area, 2048, (uintptr_t)load_area + 2048, 1024);
}

static uint32_t expected_sum(const uint8_t *data, size_t length)
{
    uint32_t sum = 0;

    for (size_t i = 0; i < length; i++)
        sum = sum * 31 + data[i];
    return sum;
}

static void test_load_and_hash(void **state)
{
    uint8_t digest[64];
    uint8_t expected[32];
    (void)state;

    load_setup();
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_f, digest, sizeof(digest)),
                     PB_OK);

    /* Read in chunks of 1 KiB: two for the kernel, one for the dtb */
    assert_int_equal(reads, 3);
    assert_memory_equal(load_area, payload, sizeof(payload));
    assert_int_equal(fake.hashed_bytes, sizeof(payload));
    /* One result per part */
    assert_int_equal(results, 2);
    assert_int_equal(last_result, PB_OK);

    fake_digest(expected, sizeof(expected), expected_sum(payload, sizeof(payload)));
    assert_memory_equal(digest, expected, sizeof(expected));
}

/*
 * Without read_f the image is already at its load address (in place): what
 * is hashed is what is there, nothing is copied and nothing else is read.
 */
static void test_load_and_hash_in_memory(void **state)
{
    static uint8_t at_load_address[sizeof(load_area)];
    uint8_t digest[64];
    uint8_t expected[32];
    uint8_t changed[64];
    (void)state;

    /* Other bytes at the load address than in the payload's storage, so
     * that the digest tells which of the two was hashed */
    load_setup();
    for (size_t i = 0; i < sizeof(load_area); i++)
        load_area[i] = (uint8_t)(i * 13 + 1);
    memcpy(at_load_address, load_area, sizeof(load_area));

    assert_int_equal(boot_image_load_and_hash(hdr, 4096, NULL, NULL, digest, sizeof(digest)),
                     PB_OK);
    assert_int_equal(fake.hashed_bytes, sizeof(load_area));
    assert_int_equal(reads, 0);
    assert_memory_equal(load_area, at_load_address, sizeof(load_area));

    fake_digest(expected, sizeof(expected), expected_sum(load_area, sizeof(load_area)));
    assert_memory_equal(digest, expected, sizeof(expected));
    fake_digest(expected, sizeof(expected), expected_sum(payload, sizeof(payload)));
    assert_memory_not_equal(digest, expected, sizeof(expected));

    /* The payload's storage has no say */
    payload[100] ^= 0x01;
    assert_int_equal(boot_image_load_and_hash(hdr, 4096, NULL, NULL, changed, sizeof(changed)),
                     PB_OK);
    assert_memory_equal(changed, digest, 32);

    /* One bit at the load address, in each part and in the last byte */
    static const size_t offsets[] = { 0, 2047, 2048, sizeof(load_area) - 1 };

    for (size_t i = 0; i < ARRAY_SIZE(offsets); i++) {
        load_area[offsets[i]] ^= 0x01;
        assert_int_equal(boot_image_load_and_hash(hdr, 4096, NULL, NULL, changed, sizeof(changed)),
                         PB_OK);
        assert_memory_not_equal(changed, digest, 32);
        load_area[offsets[i]] ^= 0x01;
    }

    /* In chunks, as the boot does it: the same digest */
    assert_int_equal(boot_image_load_and_hash(hdr, 512, NULL, NULL, changed, sizeof(changed)),
                     PB_OK);
    assert_memory_equal(changed, digest, 32);
}

static void test_load_and_hash_digest_sizes(void **state)
{
    uint8_t digest[64];
    (void)state;

    load_setup();
    hdr->hash_kind = BPAK_HASH_SHA384;
    assert_int_equal(boot_image_load_and_hash(hdr, 4096, read_f, NULL, digest, 47), -PB_ERR_PARAM);
    assert_int_equal(boot_image_load_and_hash(hdr, 4096, read_f, NULL, digest, 48), PB_OK);
    assert_int_equal(fake.alg, HASH_SHA384);

    hdr->hash_kind = BPAK_HASH_SHA512;
    assert_int_equal(boot_image_load_and_hash(hdr, 4096, read_f, NULL, digest, 63), -PB_ERR_PARAM);
    assert_int_equal(boot_image_load_and_hash(hdr, 4096, read_f, NULL, digest, 64), PB_OK);
    assert_int_equal(fake.alg, HASH_SHA512);

    hdr->hash_kind = BPAK_HASH_SHA256;
    assert_int_equal(boot_image_load_and_hash(hdr, 4096, read_f, NULL, digest, 31), -PB_ERR_PARAM);
}

static void test_load_and_hash_errors(void **state)
{
    uint8_t digest[64];
    struct bpak_meta_header *meta;
    (void)state;

    load_setup();
    hdr->magic = 0;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64),
                     -PB_ERR_BAD_HEADER);

    load_setup();
    hdr->hash_kind = 99;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64),
                     -PB_ERR_UNKNOWN_HASH);

    load_setup();
    fake.init_rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64),
                     -PB_ERR_NOT_SUPPORTED);
    fake.init_rc = PB_OK;

    /* The second chunk of the kernel can't be read */
    load_setup();
    fail_read_at = 2;
    read_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64), -PB_ERR_IO);
    assert_int_equal(reads, 2);

    load_setup();
    fake.update_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64), -PB_ERR_IO);
    fake.update_rc = PB_OK;

    /* The result callback can stop the load */
    load_setup();
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_f, digest, 64), PB_OK);
    load_setup();
    fail_read_at = 1;
    read_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_f, digest, 64), -PB_ERR_IO);
    assert_int_equal(results, 1);
    assert_int_equal(last_result, -PB_ERR_IO);

    /* A hash error while a host streams the image: the error is kept, and
     * the rest is still read with a result for every part (F-061) */
    load_setup();
    fake.update_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_sent_f, digest, 64),
                     -PB_ERR_IO);
    assert_int_equal(reads, 3);
    assert_int_equal(results, 2);
    assert_int_equal(last_result, -PB_ERR_IO);
    fake.update_rc = PB_OK;

    /* A read error ends the load, after its result */
    load_setup();
    fail_read_at = 2;
    read_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_sent_f, digest, 64),
                     -PB_ERR_IO);
    assert_int_equal(reads, 2);
    assert_int_equal(results, 1);
    assert_int_equal(last_result, -PB_ERR_IO);

    /* The dtb has no load address */
    load_setup();
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64),
                     -PB_ERR_BAD_META);
}

/* What load_setup() makes, with other sizes: the kernel, and the dtb right
 * after it when it has a size */
static void load_setup_sizes(size_t kernel_size, size_t dtb_size)
{
    load_setup();
    make_header(BPAK_HASH_SHA256,
                (uintptr_t)load_area,
                kernel_size,
                (uintptr_t)load_area + kernel_size,
                dtb_size);
}

/*
 * The watchdog is kicked once per chunk (platform.md 3.3, boot.md 4.2 step
 * 5, F-243): a chunk is at most load_chunk_size and never spans two parts.
 * The same for every source: a partition's read function, the host's stream
 * (a result function too) and in place (no read function).
 */
static void test_load_and_hash_kicks_per_chunk(void **state)
{
    static const struct {
        size_t kernel_size;
        size_t dtb_size;
        size_t chunk_size;
        unsigned int kicks;
    } cases[] = {
        /* A part of exactly one chunk */
        { 1024, 0, 1024, 1 },
        /* Smaller than a chunk (a part is a multiple of 512 bytes) */
        { 512, 0, 1024, 1 },
        /* Exactly two and three chunks */
        { 2048, 0, 1024, 2 },
        { 3072, 0, 1024, 3 },
        /* A short last chunk: 1024 + 1024 + 512, and 1000 + 24 */
        { 2560, 0, 1024, 3 },
        { 1536, 0, 1024, 2 },
        { 1024, 0, 1000, 2 },
        /* Two parts: the chunks of each */
        { 2048, 1024, 1024, 3 },
        { 1024, 1024, 1024, 2 },
        /* Two parts in less than one chunk: one each, a chunk is of one part */
        { 2048, 1024, 4096, 2 },
        { 512, 512, 1024, 2 },
        /* Short last chunks in both */
        { 1536, 1536, 1024, 4 },
        /* A part without bytes has no chunk */
        { 0, 1024, 1024, 1 },
        /* All of it in one chunk, and many small ones */
        { 3072, 0, 4096, 1 },
        { 512, 512, 64, 16 },
    };

    static const struct {
        boot_read_cb_t read_f;
        boot_result_cb_t result_f;
    } sources[] = {
        { read_f, NULL }, /* A partition */
        { read_f, result_sent_f }, /* The host's stream */
        { NULL, NULL }, /* In place */
    };

    uint8_t digest[64];
    (void)state;

    for (size_t s = 0; s < ARRAY_SIZE(sources); s++) {
        for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
            load_setup_sizes(cases[i].kernel_size, cases[i].dtb_size);
            assert_int_equal(boot_image_load_and_hash(hdr,
                                                      cases[i].chunk_size,
                                                      sources[s].read_f,
                                                      sources[s].result_f,
                                                      digest,
                                                      sizeof(digest)),
                             PB_OK);
            assert_int_equal(fake_wdog_kicks, cases[i].kicks);
            /* One per chunk that was hashed, and per read where there is one */
            assert_int_equal(fake.updates, cases[i].kicks);
            assert_int_equal(reads, sources[s].read_f ? (int)cases[i].kicks : 0);
            assert_int_equal(fake.hashed_bytes, cases[i].kernel_size + cases[i].dtb_size);
        }
    }

    /* An image without parts has no chunk: no kick */
    load_setup();
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, sizeof(digest)),
                     PB_OK);
    assert_int_equal(fake_wdog_kicks, 0);
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_sent_f, digest, 64), PB_OK);
    assert_int_equal(fake_wdog_kicks, 0);
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, NULL, NULL, digest, sizeof(digest)),
                     PB_OK);
    assert_int_equal(fake_wdog_kicks, 0);
    assert_int_equal(reads, 0);
}

/*
 * The kick comes after the chunk, when it is read and its hash is started:
 * it says that the load made progress. So the first chunk is read and hashed
 * under the kick that came before the load, and the last kick is the last
 * chunk's: hash_final() and what follows have the one before the jump.
 */
static void test_load_and_hash_kicks_after_the_chunk(void **state)
{
    uint8_t digest[64];
    (void)state;

    /* Three chunks: two of the kernel, one of the dtb */
    load_setup();
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, sizeof(digest)),
                     PB_OK);
    assert_int_equal(fake_wdog_kicks, 3);

    for (unsigned int chunk = 0; chunk < 3; chunk++) {
        assert_int_equal(kicks_at_read[chunk], chunk);
        assert_int_equal(fake.kicks_at_update[chunk], chunk);
    }

    /* In place: the same, without the reads */
    load_setup();
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, NULL, NULL, digest, sizeof(digest)),
                     PB_OK);
    assert_int_equal(fake_wdog_kicks, 3);

    for (unsigned int chunk = 0; chunk < 3; chunk++)
        assert_int_equal(fake.kicks_at_update[chunk], chunk);
}

/* A chunk that failed isn't kicked for: the count is the chunks before it */
static void test_load_and_hash_kicks_until_an_error(void **state)
{
    static const boot_result_cb_t result_fs[] = { NULL, result_sent_f, result_f };
    uint8_t digest[64];
    struct bpak_meta_header *meta;
    (void)state;

    /* Nothing is read before these: no kick */
    load_setup();
    hdr->magic = 0;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64),
                     -PB_ERR_BAD_HEADER);
    assert_int_equal(fake_wdog_kicks, 0);

    load_setup();
    hdr->hash_kind = 99;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64),
                     -PB_ERR_UNKNOWN_HASH);
    assert_int_equal(fake_wdog_kicks, 0);

    load_setup();
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 31), -PB_ERR_PARAM);
    assert_int_equal(fake_wdog_kicks, 0);

    load_setup();
    fake.init_rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64),
                     -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(fake_wdog_kicks, 0);
    fake.init_rc = PB_OK;

    /* A read error at the first, the second and the third chunk, which is
     * the second part's first: with and without a result function */
    for (size_t r = 0; r < ARRAY_SIZE(result_fs); r++) {
        for (int failing = 1; failing <= 3; failing++) {
            load_setup();
            fail_read_at = failing;
            read_rc = -PB_ERR_IO;
            assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_fs[r], digest, 64),
                             -PB_ERR_IO);
            assert_int_equal(reads, failing);
            assert_int_equal(fake_wdog_kicks, failing - 1);
        }
    }

    /* A hash error ends a partition's load and an in place one at once: the
     * chunk that failed has no kick */
    load_setup();
    fake.update_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64), -PB_ERR_IO);
    assert_int_equal(reads, 1);
    assert_int_equal(fake_wdog_kicks, 0);

    load_setup();
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, NULL, NULL, digest, 64), -PB_ERR_IO);
    assert_int_equal(fake_wdog_kicks, 0);

    /* The host's stream is read to its end after a hash error (F-061), and
     * every chunk that is read is progress: a large image that fails early
     * doesn't end in a reset before the host has its results */
    load_setup();
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_sent_f, digest, 64),
                     -PB_ERR_IO);
    assert_int_equal(reads, 3);
    assert_int_equal(results, 2);
    assert_int_equal(fake_wdog_kicks, 3);
    fake.update_rc = PB_OK;

    /* A result that can't be sent ends the load after the first part */
    load_setup();
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_fails_f, digest, 64),
                     -PB_ERR_IO);
    assert_int_equal(results, 1);
    assert_int_equal(fake_wdog_kicks, 2);

    /* The dtb has no load address: the kernel's chunks, then the error */
    load_setup();
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64),
                     -PB_ERR_BAD_META);
    assert_int_equal(fake_wdog_kicks, 2);

    /* A failed final hash: every chunk was loaded */
    load_setup();
    fake.final_rc = -PB_ERR_IO;
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, 64), -PB_ERR_IO);
    assert_int_equal(fake_wdog_kicks, 3);
    fake.final_rc = PB_OK;
}

static void test_copy_and_hash(void **state)
{
    uint8_t digest[64];
    uint8_t expected[32];
    (void)state;

    load_setup();
    assert_int_equal(boot_image_copy_and_hash(hdr,
                                              (uintptr_t)payload,
                                              sizeof(payload),
                                              (uintptr_t)load_area,
                                              sizeof(load_area),
                                              digest,
                                              sizeof(digest)),
                     PB_OK);
    assert_memory_equal(load_area, payload, sizeof(payload));
    fake_digest(expected, sizeof(expected), expected_sum(payload, sizeof(payload)));
    assert_memory_equal(digest, expected, sizeof(expected));

    hdr->hash_kind = BPAK_HASH_SHA384;
    assert_int_equal(boot_image_copy_and_hash(hdr,
                                              (uintptr_t)payload,
                                              sizeof(payload),
                                              (uintptr_t)load_area,
                                              sizeof(load_area),
                                              digest,
                                              48),
                     PB_OK);
    hdr->hash_kind = BPAK_HASH_SHA512;
    assert_int_equal(boot_image_copy_and_hash(hdr,
                                              (uintptr_t)payload,
                                              sizeof(payload),
                                              (uintptr_t)load_area,
                                              sizeof(load_area),
                                              digest,
                                              64),
                     PB_OK);
    assert_int_equal(fake.alg, HASH_SHA512);
}

static void test_copy_and_hash_errors(void **state)
{
    uint8_t digest[64];
    const uintptr_t src = (uintptr_t)payload;
    const uintptr_t dst = (uintptr_t)load_area;
    (void)state;

    load_setup();
    hdr->magic = 0;
    assert_int_equal(boot_image_copy_and_hash(hdr, src, 0, dst, 0, digest, 64), -PB_ERR_BAD_HEADER);

    load_setup();
    hdr->hash_kind = 99;
    assert_int_equal(boot_image_copy_and_hash(hdr, src, 0, dst, 0, digest, 64),
                     -PB_ERR_UNKNOWN_HASH);

    load_setup();
    assert_int_equal(boot_image_copy_and_hash(hdr, src, 0, dst, 0, digest, 31), -PB_ERR_PARAM);

    fake.init_rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(boot_image_copy_and_hash(hdr, src, 0, dst, 0, digest, 64),
                     -PB_ERR_NOT_SUPPORTED);
    fake.init_rc = PB_OK;

    fake.update_rc = -PB_ERR_IO;
    assert_int_equal(
        boot_image_copy_and_hash(hdr, src, sizeof(payload), dst, sizeof(load_area), digest, 64),
        -PB_ERR_IO);
    fake.update_rc = PB_OK;

    /* A part that doesn't fit in the source or the destination is not copied */
    load_setup();
    assert_int_equal(
        boot_image_copy_and_hash(hdr, src, sizeof(payload) - 1, dst, sizeof(load_area), digest, 64),
        -PB_ERR_BUF_TOO_SMALL);
    assert_int_equal(
        boot_image_copy_and_hash(hdr, src, sizeof(payload), dst, sizeof(load_area) - 1, digest, 64),
        -PB_ERR_BUF_TOO_SMALL);
    assert_int_equal(load_area[sizeof(load_area) - 1], 0);
}

static void test_verify_payload(void **state)
{
    static const struct {
        uint8_t kind;
        size_t length;
    } kinds[] = {
        { BPAK_HASH_SHA256, 32 },
        { BPAK_HASH_SHA384, 48 },
        { BPAK_HASH_SHA512, 64 },
    };

    uint8_t digest[64];
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(kinds); i++) {
        hdr->hash_kind = kinds[i].kind;
        memset(hdr->payload_hash, 0x11, sizeof(hdr->payload_hash));
        memset(digest, 0x11, sizeof(digest));
        /* Bytes past the digest length don't matter */
        if (kinds[i].length < sizeof(digest))
            digest[kinds[i].length] = 0x22;
        assert_int_equal(boot_image_verify_payload(hdr, digest), PB_OK);

        digest[kinds[i].length - 1] = 0x22;
        assert_int_equal(boot_image_verify_payload(hdr, digest), -PB_ERR_BAD_PAYLOAD);
    }

    hdr->hash_kind = 99;
    assert_int_equal(boot_image_verify_payload(hdr, digest), -PB_ERR_UNKNOWN_HASH);

    hdr->magic = 0;
    assert_int_equal(boot_image_verify_payload(hdr, digest), -PB_ERR_BAD_HEADER);
}

/* The dtb is outside the payload's hash: bpak's dont-hash flag */
static void exclude_dtb_from_hash(void)
{
    struct bpak_part_header *part;

    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    part->flags |= BPAK_FLAG_EXCLUDE_FROM_HASH;
}

/*
 * boot.md 4.2, security.md 4.2 (D-035, F-076): an image with a part outside
 * the payload's hash is refused in step 4, loaded or in place, with an error
 * of its own and before anything is read. Nothing would authenticate the
 * part's bytes.
 */
static void test_part_outside_the_hash_is_refused(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    /* The image of the suite, which is placed */
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);

    exclude_dtb_from_hash();
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(fake.updates, 0);

    /* In place */
    make_header(BPAK_HASH_SHA256, FLASH_BASE, 1024, FLASH_BASE + 0x1000, 512);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE), PB_OK);
    exclude_dtb_from_hash();
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE),
                     -PB_ERR_NOT_SUPPORTED);

    /* Whatever else is wrong with the image: a part that is placed nowhere,
     * and the first part as well as the last */
    make_header(BPAK_HASH_SHA256, PB_START, 1024, FREE_RAM, 512);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    assert_int_equal(bpak_get_part(hdr, ID_KERNEL, &part), BPAK_OK);
    part->flags |= BPAK_FLAG_EXCLUDE_FROM_HASH;
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_NOT_SUPPORTED);

    /* The other flag of the format is no reason */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, FREE_RAM + 0x100000, 512);
    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    part->flags |= BPAK_FLAG_TRANSPORT;
    part->transport_size = 512;
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);
}

/*
 * Why the refusal breaks no image that booted (D-035, compatibility.md):
 * upstream's hash leaves the part out, pb's never did. Every release hashed
 * every part it loaded, so the digest of such an image was never the
 * header's and the boot failed with -PB_ERR_BAD_PAYLOAD, after the load. The
 * load still hashes every part: one that got past step 4 isn't loaded
 * unhashed.
 */
static void test_part_outside_the_hash_never_verified(void **state)
{
    uint8_t digest[64];
    (void)state;

    /* As upstream makes it: the header's hash is over the kernel alone */
    load_setup();
    fake_digest(hdr->payload_hash, 32, expected_sum(payload, 2048));
    exclude_dtb_from_hash();

    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, result_f, digest, sizeof(digest)),
                     PB_OK);
    assert_int_equal(fake.hashed_bytes, sizeof(payload));
    assert_int_equal(boot_image_verify_payload(hdr, digest), -PB_ERR_BAD_PAYLOAD);

    /* And in place */
    memcpy(load_area, payload, sizeof(payload));
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, NULL, NULL, digest, sizeof(digest)),
                     PB_OK);
    assert_int_equal(fake.hashed_bytes, sizeof(payload));
    assert_int_equal(boot_image_verify_payload(hdr, digest), -PB_ERR_BAD_PAYLOAD);

    /* The hash is the image's: with the part in it, pb's digest is the header's */
    fake_digest(hdr->payload_hash, 32, expected_sum(payload, sizeof(payload)));
    assert_int_equal(boot_image_verify_payload(hdr, digest), PB_OK);
}

/* The kernel's load address meta, with 'size' bytes of its value's and 0xff
 * in the bytes after them, up to the eight a u64 has: another value's
 * bytes, where a header bpak made has zeros */
static void resize_kernel_load_addr(uint16_t size)
{
    struct bpak_meta_header *meta;

    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_KERNEL, &meta), BPAK_OK);
    meta->size = size;

    if (size < 8)
        memset(&hdr->metadata[meta->offset + size], 0xff, 8 - size);

    assert_int_equal(bpak_valid_header(hdr), BPAK_OK);
}

/*
 * F-065: the load address is made of the meta's own bytes, whatever the
 * meta's size. It was read as a u64 without a look at the size: a shorter
 * meta took the bytes after it into the address.
 */
static void test_load_addr_is_the_metas_own_bytes(void **state)
{
    uint64_t addr = 0;
    (void)state;

    /* A u64, as every release of the bpak CLI wrote it: as it was */
    make_header(BPAK_HASH_SHA256, 0x123456789abcdef0, 1024, 0, 0);
    assert_int_equal(boot_image_load_addr(hdr, ID_KERNEL, &addr), PB_OK);
    assert_true(addr == 0x123456789abcdef0);

    /* Longer: its first eight bytes, as it was */
    resize_kernel_load_addr(16);
    memset(&hdr->metadata[8], 0xee, 8);
    assert_int_equal(boot_image_load_addr(hdr, ID_KERNEL, &addr), PB_OK);
    assert_true(addr == 0x123456789abcdef0);

    /* Shorter: its bytes, zero extended, and not the bytes after it. They
     * made 0xffffffff60080000 of it, in no partition */
    make_header(BPAK_HASH_SHA256, FLASH_BASE, 1024, 0, 0);
    resize_kernel_load_addr(4);
    assert_int_equal(boot_image_load_addr(hdr, ID_KERNEL, &addr), PB_OK);
    assert_true(addr == FLASH_BASE);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, FLASH_BASE, FLASH_SIZE), PB_OK);

    resize_kernel_load_addr(3);
    assert_int_equal(boot_image_load_addr(hdr, ID_KERNEL, &addr), PB_OK);
    assert_true(addr == (FLASH_BASE & 0xffffff));

    /* The same for an image that is loaded */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, 0, 0);
    resize_kernel_load_addr(4);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);

    /* A part without a load address isn't one of them */
    assert_int_equal(boot_image_load_addr(hdr, ID_DTB, &addr), -PB_ERR_BAD_META);
}

/* F-065: a meta without bytes holds no address. Eight bytes that weren't its
 * own were read: the next value's, or what follows the metadata */
static void test_load_addr_without_bytes_is_none(void **state)
{
    uint8_t digest[64];
    uint64_t addr = 77;
    (void)state;

    load_setup();
    resize_kernel_load_addr(0);

    assert_int_equal(boot_image_load_addr(hdr, ID_KERNEL, &addr), -PB_ERR_BAD_META);
    assert_true(addr == 77);
    assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_BAD_META);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, (uintptr_t)load_area, sizeof(load_area)),
                     -PB_ERR_BAD_META);
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, sizeof(digest)),
                     -PB_ERR_BAD_META);
    assert_int_equal(reads, 0);
}

/* F-065: the load goes to the address the placement looked at */
static void test_load_addr_is_where_the_part_is_loaded(void **state)
{
    uint8_t digest[64];
    (void)state;

    /* The test's load area is below 4 GiB (-no-pie): four bytes hold it */
    assert_true((uintptr_t)load_area <= UINT32_MAX);

    load_setup();
    resize_kernel_load_addr(4);
    assert_int_equal(boot_image_verify_parts_in_place(hdr, (uintptr_t)load_area, sizeof(load_area)),
                     PB_OK);
    assert_int_equal(boot_image_load_and_hash(hdr, 1024, read_f, NULL, digest, sizeof(digest)),
                     PB_OK);
    assert_memory_equal(load_area, payload, sizeof(payload));
}

#define IMG_TEST(f) cmocka_unit_test_setup(f, setup)

int main(void)
{
    const struct CMUnitTest tests[] = {
        IMG_TEST(test_auth_header),
        IMG_TEST(test_auth_header_errors),
        IMG_TEST(test_verify_parts),
        IMG_TEST(test_verify_parts_needs_load_addr),
        IMG_TEST(test_verify_parts_inside_a_region),
        IMG_TEST(test_verify_parts_outside_the_regions),
        IMG_TEST(test_verify_parts_across_two_regions),
        IMG_TEST(test_verify_parts_wraps),
        IMG_TEST(test_verify_parts_over_each_other),
        IMG_TEST(test_verify_parts_without_regions),
        IMG_TEST(test_in_regions),
        IMG_TEST(test_regions_overlap_pb),
        IMG_TEST(test_regions_empty_or_wrapping),
        IMG_TEST(test_regions_overlap_handover),
        IMG_TEST(test_verify_parts_in_place),
        IMG_TEST(test_verify_parts_in_place_ignores_pb_memory),
        IMG_TEST(test_verify_parts_in_place_outside),
        IMG_TEST(test_verify_parts_in_place_errors),
        IMG_TEST(test_load_and_hash),
        IMG_TEST(test_load_and_hash_in_memory),
        IMG_TEST(test_load_and_hash_digest_sizes),
        IMG_TEST(test_load_and_hash_errors),
        IMG_TEST(test_load_and_hash_kicks_per_chunk),
        IMG_TEST(test_load_and_hash_kicks_after_the_chunk),
        IMG_TEST(test_load_and_hash_kicks_until_an_error),
        IMG_TEST(test_copy_and_hash),
        IMG_TEST(test_copy_and_hash_errors),
        IMG_TEST(test_verify_payload),
        IMG_TEST(test_part_outside_the_hash_is_refused),
        IMG_TEST(test_part_outside_the_hash_never_verified),
        IMG_TEST(test_load_addr_is_the_metas_own_bytes),
        IMG_TEST(test_load_addr_without_bytes_is_none),
        IMG_TEST(test_load_addr_is_where_the_part_is_loaded),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
