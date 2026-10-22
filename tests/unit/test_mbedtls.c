/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Mbed TLS crypto provider (src/drivers/crypto/mbedtls) through the crypto
 * API, with the keys from the internal keystore and the host auth token.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <bpak/keystore.h>
#include <drivers/crypto/mbedtls.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>

extern const struct bpak_keystore keystore_pb;

#define DEV_KEY_ID        0xa90f9680 /* pb-development, prime256v1 */

/* tests/<uuid>.token signs the sha256 of this string with pb-development */
#define TOKEN_DEVICE_UUID "02e49231-756e-35ee-a982-378e5ba866a9"

static const uint8_t md5_abc[] = { 0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
                                   0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72 };

static const uint8_t sha256_abc[] = { 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
                                      0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                      0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
                                      0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad };

static const uint8_t sha384_abc[] = { 0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b, 0xb5, 0xa0,
                                      0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07, 0x27, 0x2c, 0x32, 0xab,
                                      0x0e, 0xde, 0xd1, 0x63, 0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff,
                                      0x5b, 0xed, 0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
                                      0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7 };

static const uint8_t sha512_abc[] = {
    0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba, 0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
    0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2, 0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
    0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8, 0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
    0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e, 0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f,
};

static uint8_t token[128];
static size_t token_length;

static const struct bpak_key *find_key(uint32_t id)
{
    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id == id)
            return keystore_pb.keys[i];
    }
    return NULL;
}

static int group_setup(void **state)
{
    (void)state;

    if (mbedtls_pb_init() != PB_OK)
        return -1;

    FILE *fp = fopen(PB_AUTH_TOKEN_PATH, "rb");
    if (fp == NULL)
        return -1;
    token_length = fread(token, 1, sizeof(token), fp);
    fclose(fp);
    return (token_length > 0) ? 0 : -1;
}

static void check_digest(hash_t alg, const uint8_t *expected, size_t length)
{
    uint8_t digest[CRYPTO_MD_MAX_SZ];

    assert_int_equal(hash_init(alg), PB_OK);
    /* Split, to check that updates are chained */
    assert_int_equal(hash_update("a", 1), PB_OK);
    assert_int_equal(hash_update("bc", 2), PB_OK);
    assert_int_equal(hash_final(digest, length), PB_OK);
    assert_memory_equal(digest, expected, length);

    /* The output buffer must fit the digest */
    assert_int_equal(hash_init(alg), PB_OK);
    assert_int_equal(hash_final(digest, length - 1), -PB_ERR_BUF_TOO_SMALL);
}

static void test_digests(void **state)
{
    (void)state;

    check_digest(HASH_MD5, md5_abc, sizeof(md5_abc));
    check_digest(HASH_SHA256, sha256_abc, sizeof(sha256_abc));
    check_digest(HASH_SHA384, sha384_abc, sizeof(sha384_abc));
    check_digest(HASH_SHA512, sha512_abc, sizeof(sha512_abc));
}

static void test_copy_update_and_async_fallbacks(void **state)
{
    uint8_t dst[3];
    uint8_t digest[32];
    (void)state;

    /* The provider has neither, so the crypto API falls back to update */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_copy_update("ab", dst, 2), PB_OK);
    assert_memory_equal(dst, "ab", 2);
    assert_int_equal(hash_update_async("c", 1), PB_OK);
    assert_int_equal(hash_final(digest, sizeof(digest)), PB_OK);
    assert_memory_equal(digest, sha256_abc, sizeof(digest));
}

static void test_unknown_alg(void **state)
{
    uint8_t digest[64];
    (void)state;

    /* Two algorithms at once matches the provider, which rejects it */
    assert_int_equal(hash_init(HASH_SHA256 | HASH_SHA512), -PB_ERR_PARAM);
    assert_int_equal(hash_update("abc", 3), -PB_ERR_PARAM);
    assert_int_equal(hash_final(digest, sizeof(digest)), -PB_ERR_PARAM);

    assert_int_equal(hash_init(HASH_MD5_BROKEN), -PB_ERR_NOT_SUPPORTED);
}

static void token_digest(uint8_t *digest)
{
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(TOKEN_DEVICE_UUID, strlen(TOKEN_DEVICE_UUID)), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
}

static void test_verify_auth_token(void **state)
{
    const struct bpak_key *key = find_key(DEV_KEY_ID);
    uint8_t digest[32];
    bool verified = false;
    (void)state;

    assert_non_null(key);
    token_digest(digest);

    assert_int_equal(dsa_verify(DSA_EC_SECP256r1,
                                token,
                                token_length,
                                key->data,
                                key->size,
                                HASH_SHA256,
                                digest,
                                sizeof(digest),
                                &verified),
                     PB_OK);
    assert_true(verified);
}

static void test_verify_rejects_wrong_digest(void **state)
{
    const struct bpak_key *key = find_key(DEV_KEY_ID);
    uint8_t digest[32];
    bool verified = true;
    (void)state;

    token_digest(digest);
    digest[0] ^= 1;

    assert_int_equal(dsa_verify(DSA_EC_SECP256r1,
                                token,
                                token_length,
                                key->data,
                                key->size,
                                HASH_SHA256,
                                digest,
                                sizeof(digest),
                                &verified),
                     -PB_ERR_SIGNATURE);
    assert_false(verified);
}

static void test_verify_rejects_bad_key(void **state)
{
    const struct bpak_key *key = find_key(DEV_KEY_ID);
    uint8_t bad_key[128];
    uint8_t digest[32];
    bool verified = true;
    (void)state;

    token_digest(digest);
    memcpy(bad_key, key->data, key->size);
    bad_key[0] = 0x31;

    assert_int_equal(dsa_verify(DSA_EC_SECP256r1,
                                token,
                                token_length,
                                bad_key,
                                key->size,
                                HASH_SHA256,
                                digest,
                                sizeof(digest),
                                &verified),
                     -PB_ERR_SIGNATURE);
    assert_false(verified);
}

static void test_verify_rejects_unsupported_digest(void **state)
{
    const struct bpak_key *key = find_key(DEV_KEY_ID);
    uint8_t digest[32] = { 0 };
    bool verified = true;
    (void)state;

    assert_int_equal(dsa_verify(DSA_EC_SECP256r1,
                                token,
                                token_length,
                                key->data,
                                key->size,
                                HASH_MD5,
                                digest,
                                16,
                                &verified),
                     -PB_ERR_PARAM);
    assert_false(verified);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_digests),
        cmocka_unit_test(test_copy_update_and_async_fallbacks),
        cmocka_unit_test(test_unknown_alg),
        cmocka_unit_test(test_verify_auth_token),
        cmocka_unit_test(test_verify_rejects_wrong_digest),
        cmocka_unit_test(test_verify_rejects_bad_key),
        cmocka_unit_test(test_verify_rejects_unsupported_digest),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
