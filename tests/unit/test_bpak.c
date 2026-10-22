/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The vendored subset of the bpak library (src/lib/bpak.c).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <bpak/bpak.h>
#include <stdlib.h>
#include <string.h>

#define ID_KERNEL    0xec103b08
#define ID_DTB       0x56f91b86
#define ID_META_A    0x11111111
#define ID_META_B    0x22222222
#define ID_META_C    0x33333333

/* id("bpak-transport") */
#define ID_TRANSPORT 0x2d44bbfb

static int setup(void **state)
{
    struct bpak_header *hdr = malloc(sizeof(*hdr));
    assert_non_null(hdr);
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    *state = hdr;
    return 0;
}

static int teardown(void **state)
{
    free(*state);
    return 0;
}

static struct bpak_meta_header *
add_meta(struct bpak_header *hdr, bpak_id_t id, bpak_id_t part_ref, uint16_t size, uint8_t fill)
{
    struct bpak_meta_header *meta = NULL;

    assert_int_equal(bpak_add_meta(hdr, id, part_ref, size, &meta), BPAK_OK);
    assert_non_null(meta);
    memset(bpak_get_meta_ptr(hdr, meta, uint8_t), fill, size);
    return meta;
}

static void test_init_header(void **state)
{
    struct bpak_header *hdr = *state;

    assert_int_equal(hdr->magic, BPAK_HEADER_MAGIC);
    assert_int_equal(hdr->hash_kind, BPAK_HASH_SHA256);
    assert_int_equal(hdr->signature_kind, BPAK_SIGN_PRIME256v1);
    assert_int_equal(hdr->alignment, BPAK_PART_ALIGN);
    assert_int_equal(bpak_valid_header(hdr), BPAK_OK);
}

static void test_meta_offsets_are_aligned(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_meta_header *a = add_meta(hdr, ID_META_A, 0, 3, 0xaa);
    struct bpak_meta_header *b = add_meta(hdr, ID_META_B, 0, 9, 0xbb);
    struct bpak_meta_header *c = add_meta(hdr, ID_META_C, 0, 8, 0xcc);

    assert_int_equal(a->offset, 0);
    assert_int_equal(b->offset, BPAK_META_ALIGN);
    assert_int_equal(c->offset, 3 * BPAK_META_ALIGN);
    assert_int_equal(bpak_valid_header(hdr), BPAK_OK);
}

static void test_get_meta(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_meta_header *meta = NULL;
    struct bpak_meta_header *kernel = add_meta(hdr, ID_META_A, ID_KERNEL, 8, 1);
    struct bpak_meta_header *dtb = add_meta(hdr, ID_META_A, ID_DTB, 8, 2);

    assert_int_equal(bpak_get_meta(hdr, ID_META_A, ID_DTB, &meta), BPAK_OK);
    assert_ptr_equal(meta, dtb);
    assert_int_equal(bpak_get_meta(hdr, ID_META_A, ID_KERNEL, &meta), BPAK_OK);
    assert_ptr_equal(meta, kernel);
    /* Any part reference, the first one wins */
    assert_int_equal(bpak_get_meta_anyref(hdr, ID_META_A, &meta), BPAK_OK);
    assert_ptr_equal(meta, kernel);

    assert_int_equal(bpak_get_meta(hdr, ID_META_A, 0, &meta), -BPAK_NOT_FOUND);
    assert_null(meta);
    assert_int_equal(bpak_get_meta_anyref(hdr, ID_META_B, &meta), -BPAK_NOT_FOUND);
    assert_null(meta);
}

static void test_add_meta_twice_fails(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_meta_header *meta = NULL;

    add_meta(hdr, ID_META_A, ID_KERNEL, 8, 0);
    assert_int_equal(bpak_add_meta(hdr, ID_META_A, ID_KERNEL, 8, &meta), -BPAK_EXISTS);
    /* Same id for another part is fine */
    assert_int_equal(bpak_add_meta(hdr, ID_META_A, ID_DTB, 8, &meta), BPAK_OK);
}

static void test_add_meta_out_of_data_space(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_meta_header *meta = NULL;

    add_meta(hdr, ID_META_A, 0, BPAK_METADATA_BYTES - 8, 0);
    assert_int_equal(bpak_add_meta(hdr, ID_META_B, 0, 16, &meta), -BPAK_NO_SPACE_LEFT);
}

static void test_add_meta_out_of_headers(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_meta_header *meta = NULL;

    for (unsigned int i = 0; i < BPAK_MAX_META; i++)
        add_meta(hdr, ID_META_A + i, 0, 8, 0);

    assert_int_equal(bpak_add_meta(hdr, ID_META_B + 0x100, 0, 8, &meta), -BPAK_NO_SPACE_LEFT);
}

static void test_del_meta_moves_data(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_meta_header *meta = NULL;
    struct bpak_meta_header *a = add_meta(hdr, ID_META_A, 0, 12, 0xaa);
    add_meta(hdr, ID_META_B, 0, 8, 0xbb);
    add_meta(hdr, ID_META_C, 0, 4, 0xcc);

    bpak_del_meta(hdr, a);

    assert_int_equal(bpak_get_meta_anyref(hdr, ID_META_A, &meta), -BPAK_NOT_FOUND);

    assert_int_equal(bpak_get_meta_anyref(hdr, ID_META_B, &meta), BPAK_OK);
    assert_ptr_equal(meta, &hdr->meta[0]);
    assert_int_equal(meta->offset, 0);
    assert_int_equal(bpak_get_meta_ptr(hdr, meta, uint8_t)[0], 0xbb);

    assert_int_equal(bpak_get_meta_anyref(hdr, ID_META_C, &meta), BPAK_OK);
    assert_ptr_equal(meta, &hdr->meta[1]);
    assert_int_equal(meta->offset, BPAK_META_ALIGN);
    assert_int_equal(bpak_get_meta_ptr(hdr, meta, uint8_t)[0], 0xcc);

    assert_int_equal(hdr->meta[2].id, 0);
    /* The freed space at the end is cleared */
    for (int i = 2 * BPAK_META_ALIGN; i < 4 * BPAK_META_ALIGN; i++)
        assert_int_equal(hdr->metadata[i], 0);
}

/* Deleting with the last meta slot in use is not tested: bpak_del_meta()
 * then leaves the last slot as it is and clears the 16 bytes after the meta
 * array, i.e. parts[0] (F-068). punchboot never deletes meta data. */

static void test_parts(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_part_header *kernel = NULL;
    struct bpak_part_header *dtb = NULL;
    struct bpak_part_header *part = NULL;

    assert_int_equal(bpak_add_part(hdr, ID_KERNEL, &kernel), BPAK_OK);
    kernel->size = 1000;
    kernel->pad_bytes = 24;
    assert_int_equal(bpak_add_part(hdr, ID_DTB, &dtb), BPAK_OK);
    dtb->size = 512;
    assert_int_equal(bpak_add_part(hdr, ID_KERNEL, &part), -BPAK_EXISTS);

    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    assert_ptr_equal(part, dtb);
    assert_int_equal(bpak_get_part(hdr, ID_META_A, &part), -BPAK_NOT_FOUND);

    assert_int_equal(bpak_part_size(kernel), 1024);
    assert_int_equal(bpak_part_size_wo_pad(kernel), 1000);
    assert_int_equal(bpak_part_offset(hdr, kernel), sizeof(*hdr));
    assert_int_equal(bpak_part_offset(hdr, dtb), sizeof(*hdr) + 1024);
    /* A part that isn't in the header: after all the others */
    struct bpak_part_header other = { .id = ID_META_A };
    assert_int_equal(bpak_part_offset(hdr, &other), sizeof(*hdr) + 1024 + 512);
    assert_int_equal(bpak_valid_header(hdr), BPAK_OK);

    bpak_del_part(hdr, kernel);
    assert_int_equal(hdr->parts[0].id, ID_DTB);
    assert_int_equal(hdr->parts[1].id, 0);
    assert_int_equal(bpak_part_offset(hdr, &hdr->parts[0]), sizeof(*hdr));
}

static void test_add_part_out_of_headers(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_part_header *part = NULL;

    for (unsigned int i = 0; i < BPAK_MAX_PARTS; i++)
        assert_int_equal(bpak_add_part(hdr, ID_META_A + i, &part), BPAK_OK);

    assert_int_equal(bpak_add_part(hdr, ID_KERNEL, &part), -BPAK_NO_SPACE_LEFT);

    bpak_del_part(hdr, &hdr->parts[BPAK_MAX_PARTS - 1]);
    assert_int_equal(hdr->parts[BPAK_MAX_PARTS - 1].id, 0);
    assert_int_equal(bpak_add_part(hdr, ID_KERNEL, &part), BPAK_OK);
}

static void test_transport_part_size(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_part_header *part = NULL;

    assert_int_equal(bpak_add_part(hdr, ID_KERNEL, &part), BPAK_OK);
    part->size = 4096;
    part->transport_size = 100;
    part->flags = BPAK_FLAG_TRANSPORT;

    assert_int_equal(bpak_part_size(part), 100);
    assert_int_equal(bpak_part_size_wo_pad(part), 100);
}

static void test_add_transport_meta(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_meta_header *meta = NULL;

    assert_int_equal(bpak_add_transport_meta(hdr, ID_KERNEL, 1, 2), BPAK_OK);
    assert_int_equal(bpak_get_meta(hdr, ID_TRANSPORT, ID_KERNEL, &meta), BPAK_OK);

    struct bpak_transport_meta *tm = bpak_get_meta_ptr(hdr, meta, struct bpak_transport_meta);
    assert_int_equal(tm->alg_id_encode, 1);
    assert_int_equal(tm->alg_id_decode, 2);

    assert_int_equal(bpak_add_transport_meta(hdr, ID_KERNEL, 1, 2), -BPAK_EXISTS);
}

static void test_valid_header_rejects(void **state)
{
    struct bpak_header *hdr = *state;
    struct bpak_part_header *part = NULL;
    struct bpak_meta_header *meta = add_meta(hdr, ID_META_A, 0, 8, 0);

    hdr->magic = 0;
    assert_int_equal(bpak_valid_header(hdr), -BPAK_BAD_MAGIC);
    hdr->magic = BPAK_HEADER_MAGIC;

    meta->size = BPAK_METADATA_BYTES;
    assert_int_equal(bpak_valid_header(hdr), BPAK_OK);
    meta->offset = 8;
    assert_int_equal(bpak_valid_header(hdr), -BPAK_SIZE_ERROR);
    meta->offset = 0;
    meta->size = 8;

    hdr->hash_kind = BPAK_HASH_INVALID;
    assert_int_equal(bpak_valid_header(hdr), -BPAK_NOT_SUPPORTED);
    hdr->hash_kind = BPAK_HASH_SHA256;

    assert_int_equal(bpak_add_part(hdr, ID_KERNEL, &part), BPAK_OK);
    part->size = 1000;
    assert_int_equal(bpak_valid_header(hdr), -BPAK_BAD_ALIGNMENT);
    part->pad_bytes = 24;
    assert_int_equal(bpak_valid_header(hdr), BPAK_OK);
}

static void test_copyz_signature(void **state)
{
    struct bpak_header *hdr = *state;
    uint8_t sig[BPAK_SIGNATURE_MAX_BYTES];
    size_t size = sizeof(sig);

    assert_int_equal(bpak_copyz_signature(hdr, sig, &size), -BPAK_SIZE_ERROR);

    hdr->signature_sz = BPAK_SIGNATURE_MAX_BYTES + 1;
    assert_int_equal(bpak_copyz_signature(hdr, sig, &size), -BPAK_SIZE_ERROR);

    memset(hdr->signature, 0x5a, 72);
    hdr->signature_sz = 72;
    size = 71;
    assert_int_equal(bpak_copyz_signature(hdr, sig, &size), -BPAK_SIZE_ERROR);

    size = sizeof(sig);
    assert_int_equal(bpak_copyz_signature(hdr, sig, &size), BPAK_OK);
    assert_int_equal(size, 72);
    assert_int_equal(sig[0], 0x5a);
    assert_int_equal(sig[71], 0x5a);
    /* The signature is zeroed in the header, so the header can be hashed */
    assert_int_equal(hdr->signature_sz, 0);
    for (size_t i = 0; i < sizeof(hdr->signature); i++)
        assert_int_equal(hdr->signature[i], 0);
}

static void test_key_ids(void **state)
{
    struct bpak_header *hdr = *state;

    assert_int_equal(bpak_set_key_id(hdr, 0xa90f9680), BPAK_OK);
    assert_int_equal(bpak_set_keystore_id(hdr, 0x365f2120), BPAK_OK);
    assert_int_equal(hdr->key_id, 0xa90f9680);
    assert_int_equal(hdr->keystore_id, 0x365f2120);
}

static void test_strings(void **state)
{
    (void)state;

    assert_string_equal(bpak_error_string(BPAK_OK), "OK");
    assert_string_equal(bpak_error_string(-BPAK_VERIFY_FAIL), "Verification failed");
    assert_string_equal(bpak_error_string(-BPAK_EXISTS), "Object exists");
    assert_string_equal(bpak_error_string(1), "Unknown");
    /* Every error code has its own string */
    for (int code = BPAK_FAILED; code <= BPAK_EXISTS; code++) {
        const char *s = bpak_error_string(-code);
        assert_string_not_equal(s, "Unknown");
        for (int other = BPAK_FAILED; other < code; other++)
            assert_string_not_equal(s, bpak_error_string(-other));
    }

    assert_string_equal(bpak_signature_kind(BPAK_SIGN_PRIME256v1), "prime256v1");
    assert_string_equal(bpak_signature_kind(BPAK_SIGN_SECP384r1), "secp384r1");
    assert_string_equal(bpak_signature_kind(BPAK_SIGN_SECP521r1), "secp521r1");
    assert_string_equal(bpak_signature_kind(BPAK_SIGN_RSA4096), "rsa4096");
    assert_string_equal(bpak_signature_kind(BPAK_SIGN_INVALID), "Unknown");

    assert_string_equal(bpak_hash_kind(BPAK_HASH_SHA256), "sha256");
    assert_string_equal(bpak_hash_kind(BPAK_HASH_SHA384), "sha384");
    assert_string_equal(bpak_hash_kind(BPAK_HASH_SHA512), "sha512");
    assert_string_equal(bpak_hash_kind(BPAK_HASH_INVALID), "Unknown");

    assert_string_equal(bpak_version(), BPAK_VERSION_STRING);
    assert_int_equal(bpak_printf(0, "ignored"), BPAK_OK);
}

#define BPAK_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        BPAK_TEST(test_init_header),
        BPAK_TEST(test_meta_offsets_are_aligned),
        BPAK_TEST(test_get_meta),
        BPAK_TEST(test_add_meta_twice_fails),
        BPAK_TEST(test_add_meta_out_of_data_space),
        BPAK_TEST(test_add_meta_out_of_headers),
        BPAK_TEST(test_del_meta_moves_data),
        BPAK_TEST(test_parts),
        BPAK_TEST(test_add_part_out_of_headers),
        BPAK_TEST(test_transport_part_size),
        BPAK_TEST(test_add_transport_meta),
        BPAK_TEST(test_valid_header_rejects),
        BPAK_TEST(test_copyz_signature),
        BPAK_TEST(test_key_ids),
        BPAK_TEST(test_strings),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
