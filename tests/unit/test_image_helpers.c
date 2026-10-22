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
 * The pb memory regions are set with --defsym, like for pb-host, see
 * tests/unit/CMakeLists.txt.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/image_helpers.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <stdlib.h>
#include <string.h>

/* Must match the --defsym values in CMakeLists.txt */
#define CODE_START    0x40000000
#define DATA_START    0x400a0000
#define BSS_START     0x400c0000
#define STACK_START   0x400e0000
#define NO_INIT_START 0x40100000
#define FREE_RAM      0x49000000

#define ID_KERNEL     0xec103b08
#define ID_DTB        0x56f91b86

#define FAKE_SIG_OK   0x5a

static struct {
    int key_status_rc;
    int get_key_rc;

    int init_rc;
    int update_rc;
    int final_rc;
    hash_t alg;
    size_t hashed_bytes;
    uint32_t sum;

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
    return fake.init_rc;
}

static int fake_update(const void *buf, size_t length)
{
    const uint8_t *p = buf;

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
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, FREE_RAM + 0x100000, 512);
    return 0;
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

static void test_verify_parts_rejects_pb_memory(void **state)
{
    static const uint64_t regions[] = {
        CODE_START + 0x1000,
        DATA_START + 0x1000,
        BSS_START + 0x1000,
        STACK_START + 0x1000,
        NO_INIT_START + 0x1000,
        /* Ends inside the code */
        CODE_START - 0x200,
    };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(regions); i++) {
        /* The second part overlaps */
        make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, regions[i], 1024);
        assert_int_equal(boot_image_verify_parts(hdr), -PB_ERR_MEM);
    }

    /* Right after the last region */
    make_header(BPAK_HASH_SHA256, FREE_RAM, 1024, NO_INIT_START + 0x20000 + 1, 1024);
    assert_int_equal(boot_image_verify_parts(hdr), PB_OK);
}

/* Fake storage: the image payload, read through read_f */
static uint8_t payload[3 * 1024];
static uint8_t load_area[3 * 1024];
static int reads;
static int read_rc;
static int fail_read_at;
static int results;
static int last_result;

static int read_f(int64_t offset, size_t length, void *buf)
{
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

static void test_load_and_hash_in_memory(void **state)
{
    uint8_t digest[64];
    (void)state;

    /* Without read_f the image is already at its load address */
    load_setup();
    memcpy(load_area, payload, sizeof(payload));
    assert_int_equal(boot_image_load_and_hash(hdr, 4096, NULL, NULL, digest, sizeof(digest)),
                     PB_OK);
    assert_int_equal(fake.hashed_bytes, sizeof(payload));
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
    assert_int_equal(boot_image_copy_and_hash(hdr, src, 0, dst, 0, digest, 64), -PB_ERR_IO);
    fake.update_rc = PB_OK;
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

#define IMG_TEST(f) cmocka_unit_test_setup(f, setup)

int main(void)
{
    const struct CMUnitTest tests[] = {
        IMG_TEST(test_auth_header),
        IMG_TEST(test_auth_header_errors),
        IMG_TEST(test_verify_parts),
        IMG_TEST(test_verify_parts_needs_load_addr),
        IMG_TEST(test_verify_parts_rejects_pb_memory),
        IMG_TEST(test_load_and_hash),
        IMG_TEST(test_load_and_hash_in_memory),
        IMG_TEST(test_load_and_hash_digest_sizes),
        IMG_TEST(test_load_and_hash_errors),
        IMG_TEST(test_copy_and_hash),
        IMG_TEST(test_copy_and_hash_errors),
        IMG_TEST(test_verify_payload),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
