/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The crypto API (src/crypto.c) with fake hash and DSA providers. The
 * provider tables can't be reset, so the tests run in order: first without
 * providers, then with the fakes registered.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/crypto/mbedtls.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <string.h>

static struct {
    hash_t init_alg;
    unsigned int updates;
    unsigned int async_updates;
    unsigned int copy_updates;
    size_t final_length;
} calls;

static int fake_init(hash_t alg)
{
    calls.init_alg = alg;
    return PB_OK;
}

static int fake_update(const void *buf, size_t length)
{
    (void)buf;
    (void)length;
    calls.updates++;
    return PB_OK;
}

static int fake_update_async(const void *buf, size_t length)
{
    (void)buf;
    (void)length;
    calls.async_updates++;
    return PB_OK;
}

static int fake_copy_update(const void *src, void *dest, size_t length)
{
    memcpy(dest, src, length);
    calls.copy_updates++;
    return PB_OK;
}

static int fake_final(uint8_t *digest_out, size_t length)
{
    memset(digest_out, 0xd1, length);
    calls.final_length = length;
    return PB_OK;
}

/* With the optional callbacks */
static const struct hash_ops fake_hash_ops = {
    .name = "fake-hash",
    .alg_bits = HASH_SHA256 | HASH_SHA384,
    .init = fake_init,
    .update = fake_update,
    .update_async = fake_update_async,
    .copy_update = fake_copy_update,
    .final = fake_final,
};

/* Without them, never registered: the table holds one provider */
static const struct hash_ops other_hash_ops = {
    .name = "other-hash",
    .alg_bits = HASH_SHA512,
    .init = fake_init,
    .update = fake_update,
    .final = fake_final,
};

static dsa_t verify_alg;

static int fake_verify(const uint8_t *der_signature,
                       size_t signature_length,
                       const uint8_t *der_key,
                       size_t key_length,
                       hash_t md_alg,
                       uint8_t *md,
                       size_t md_length,
                       bool *verified)
{
    (void)der_signature;
    (void)der_key;
    (void)key_length;
    (void)md;
    (void)md_length;
    verify_alg = md_alg;
    *verified = (signature_length == 1);
    return PB_OK;
}

static const struct dsa_ops fake_dsa_ops = {
    .name = "fake-dsa",
    .alg_bits = DSA_EC_SECP256r1,
    .verify = fake_verify,
};

static void test_without_providers(void **state)
{
    uint8_t buf[16];
    uint8_t digest[32];
    bool verified = true;
    (void)state;

    assert_int_equal(hash_init(HASH_SHA256), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(hash_update(buf, sizeof(buf)), -PB_ERR_STATE);
    assert_int_equal(hash_update_async(buf, sizeof(buf)), -PB_ERR_STATE);
    assert_int_equal(hash_copy_update(buf, buf, sizeof(buf)), -PB_ERR_STATE);
    assert_int_equal(hash_final(digest, sizeof(digest)), -PB_ERR_STATE);

    assert_int_equal(dsa_verify(DSA_EC_SECP256r1,
                                buf,
                                sizeof(buf),
                                buf,
                                sizeof(buf),
                                HASH_SHA256,
                                digest,
                                sizeof(digest),
                                &verified),
                     -PB_ERR_NOT_SUPPORTED);
}

static void test_register_providers(void **state)
{
    (void)state;

    assert_int_equal(hash_add_ops(&fake_hash_ops), PB_OK);
    assert_int_equal(dsa_add_ops(&fake_dsa_ops), PB_OK);

    /* CONFIG_CRYPTO_MAX_HASH_OPS and _DSA_OPS are 1 in the host build */
    assert_int_equal(hash_add_ops(&other_hash_ops), -PB_ERR_MEM);
    assert_int_equal(dsa_add_ops(&fake_dsa_ops), -PB_ERR_MEM);
    assert_int_equal(mbedtls_pb_init(), -PB_ERR_MEM);
}

static void test_hash_dispatch(void **state)
{
    uint8_t src[16] = { 1, 2, 3 };
    uint8_t dst[16] = { 0 };
    uint8_t digest[48];
    (void)state;

    memset(&calls, 0, sizeof(calls));

    assert_int_equal(hash_init(HASH_SHA512), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(hash_init(HASH_SHA384), PB_OK);
    assert_int_equal(calls.init_alg, HASH_SHA384);

    assert_int_equal(hash_update(src, sizeof(src)), PB_OK);
    assert_int_equal(hash_update_async(src, sizeof(src)), PB_OK);
    assert_int_equal(hash_copy_update(src, dst, sizeof(src)), PB_OK);
    assert_memory_equal(src, dst, sizeof(src));

    assert_int_equal(calls.updates, 1);
    assert_int_equal(calls.async_updates, 1);
    assert_int_equal(calls.copy_updates, 1);

    assert_int_equal(hash_final(digest, sizeof(digest)), PB_OK);
    assert_int_equal(calls.final_length, sizeof(digest));
    assert_int_equal(digest[0], 0xd1);

    /* Final ends the operation */
    assert_int_equal(hash_update(src, sizeof(src)), -PB_ERR_STATE);
}

static void test_dsa_dispatch(void **state)
{
    uint8_t sig[1] = { 0 };
    uint8_t key[1] = { 0 };
    uint8_t md[32] = { 0 };
    bool verified = false;
    (void)state;

    assert_int_equal(
        dsa_verify(
            DSA_EC_SECP384r1, sig, sizeof(sig), key, sizeof(key), HASH_SHA256, md, 32, &verified),
        -PB_ERR_NOT_SUPPORTED);

    assert_int_equal(
        dsa_verify(
            DSA_EC_SECP256r1, sig, sizeof(sig), key, sizeof(key), HASH_SHA384, md, 32, &verified),
        PB_OK);
    assert_true(verified);
    assert_int_equal(verify_alg, HASH_SHA384);

    assert_int_equal(
        dsa_verify(DSA_EC_SECP256r1, sig, 2, key, sizeof(key), HASH_SHA256, md, 32, &verified),
        PB_OK);
    assert_false(verified);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_without_providers),
        cmocka_unit_test(test_register_providers),
        cmocka_unit_test(test_hash_dispatch),
        cmocka_unit_test(test_dsa_dispatch),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
