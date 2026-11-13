/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The CAAM driver's hashing and signature verification
 * (src/drivers/crypto/caam/imx_caam.c) through the crypto API, against the
 * contract (doc/interfaces/crypto.md) and a model of the CAAM's job ring
 * (models/caam_model.h). The tests are named after the contract's sections.
 *
 * The driver keeps its state and can be set up once, so the tests share one
 * CAAM: each ends with an engine that is idle, which the teardown checks
 * after a hash of its own. HASH_MD5_BROKEN has no test: the model doesn't
 * know its digest.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "crypto_vectors.h"
#include "models/bench.h"
#include "models/caam_model.h"
#include <bpak/keystore.h>
#include <drivers/crypto/imx_caam.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <string.h>

extern const struct bpak_keystore keystore_pb;

/* Job ring 0 of the i.MX6UL */
#define CAAM_JR_BASE 0x02141000

/* No backend has this one */
#define HASH_NONE    BIT(20)

/* What the engine reads and writes is below 4 GiB (caam_model.h): static */
static uint8_t pattern[VECTOR_PATTERN_LENGTH];
static uint8_t chunk[4096];
static uint8_t digest[CRYPTO_MD_MAX_SZ];
static uint8_t signature[160];

static const struct {
    hash_t alg;
    size_t length;
    const uint8_t *abc;
    const uint8_t *pattern;
} hashes[] = {
    { HASH_MD5, 16, vector_md5_abc, vector_md5_pattern },
    { HASH_SHA256, 32, vector_sha256_abc, vector_sha256_pattern },
    { HASH_SHA384, 48, vector_sha384_abc, vector_sha384_pattern },
    { HASH_SHA512, 64, vector_sha512_abc, vector_sha512_pattern },
};

/* Command mode's token, per key kind */
static const struct token {
    dsa_t dsa;
    uint32_t key_id;
    hash_t hash;
    size_t digest_length;
    const uint8_t *signature;
    size_t signature_length;
} tokens[] = {
    { DSA_EC_SECP256r1,
      VECTOR_KEY_ID_P256,
      HASH_SHA256,
      32,
      vector_token_p256,
      sizeof(vector_token_p256) },
    { DSA_EC_SECP384r1,
      VECTOR_KEY_ID_P384,
      HASH_SHA384,
      48,
      vector_token_p384,
      sizeof(vector_token_p384) },
    { DSA_EC_SECP521r1,
      VECTOR_KEY_ID_P521,
      HASH_SHA512,
      64,
      vector_token_p521,
      sizeof(vector_token_p521) },
};

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    if (!caam_model_owns(addr) || width != 32)
        fail_msg("Unexpected %u bit read at 0x%lx", width, (unsigned long)addr);
    return caam_model_read(addr);
}

void fake_mmio_write(uintptr_t addr, uint64_t value, unsigned int width)
{
    if (!caam_model_owns(addr) || width != 32)
        fail_msg("Unexpected %u bit write at 0x%lx", width, (unsigned long)addr);
    caam_model_write(addr, (uint32_t)value);
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

static const struct bpak_key *find_key(uint32_t id)
{
    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id == id)
            return keystore_pb.keys[i];
    }
    fail_msg("No key 0x%x in the keystore", id);
    return NULL;
}

static int group_setup(void **state)
{
    static const struct caam_model_config cfg = {
        .jr_base = CAAM_JR_BASE,
        .era = 9,
    };
    (void)state;

    for (size_t i = 0; i < sizeof(pattern); i++)
        pattern[i] = (uint8_t)(i * 7 + 3);
    for (size_t i = 0; i < sizeof(chunk); i++)
        chunk[i] = (uint8_t)(i * 13 + 1);

    bench_reset();
    caam_model_init(&cfg);
    return (imx_caam_init(CAAM_JR_BASE) == PB_OK) ? 0 : -1;
}

static int setup(void **state)
{
    (void)state;
    bench_reset();
    caam_model_job_time(20);
    caam_model_hang(false);
    return 0;
}

/* After anything a test did, a hash gives the right answer and leaves the
 * engine idle (crypto.md 8), and the driver broke no rule of the engine's */
static int teardown(void **state)
{
    (void)state;
    caam_model_hang(false);

    if (hash_init(HASH_SHA256) != PB_OK || hash_update("abc", 3) != PB_OK ||
        hash_final(digest, 32) != PB_OK || memcmp(digest, vector_sha256_abc, 32) != 0) {
        print_error("The hash after the test failed\n");
        return -1;
    }

    if (!caam_model_idle()) {
        print_error("The engine isn't idle after the test\n");
        return -1;
    }

    if (bench_violations() != 0) {
        bench_dump();
        return -1;
    }
    return 0;
}

/* A verification as its users make it: the token's hash, then its signature.
 * 'good' is the token, otherwise one bit of its signature is changed */
static int verify_token(const struct token *t, bool good, bool *verified)
{
    const struct bpak_key *key = find_key(t->key_id);
    int rc;

    rc = hash_init(t->hash);

    if (rc != PB_OK)
        return rc;

    rc = hash_update(VECTOR_TOKEN_UUID, strlen(VECTOR_TOKEN_UUID));

    if (rc != PB_OK)
        return rc;

    rc = hash_final(digest, t->digest_length);

    if (rc != PB_OK)
        return rc;

    memcpy(signature, t->signature, t->signature_length);

    if (!good)
        signature[t->signature_length - 1] ^= 1;

    return dsa_verify(t->dsa,
                      signature,
                      t->signature_length,
                      key->data,
                      key->size,
                      t->hash,
                      digest,
                      t->digest_length,
                      verified);
}

/* A verification that must fail, then one that must pass */
static void assert_verdicts(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(tokens); i++) {
        bool verified = true;

        assert_int_not_equal(verify_token(&tokens[i], false, &verified), PB_OK);
        assert_false(verified);

        assert_int_equal(verify_token(&tokens[i], true, &verified), PB_OK);
        assert_true(verified);
    }
}

/* 3: the digest is that of the bytes, however they are split */
static void test_3_known_answers(void **state)
{
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(hashes); i++) {
        assert_int_equal(hash_init(hashes[i].alg), PB_OK);
        assert_int_equal(hash_update("a", 1), PB_OK);
        assert_int_equal(hash_update("bc", 2), PB_OK);
        assert_int_equal(hash_final(digest, hashes[i].length), PB_OK);
        assert_memory_equal(digest, hashes[i].abc, hashes[i].length);

        assert_int_equal(hash_init(hashes[i].alg), PB_OK);
        assert_int_equal(hash_update(pattern, sizeof(pattern)), PB_OK);
        assert_int_equal(hash_final(digest, hashes[i].length), PB_OK);
        assert_memory_equal(digest, hashes[i].pattern, hashes[i].length);

        /* Blocks, and what is left of one, from the driver's own buffer */
        assert_int_equal(hash_init(hashes[i].alg), PB_OK);
        assert_int_equal(hash_update(pattern, 100), PB_OK);
        assert_int_equal(hash_update(&pattern[100], 500), PB_OK);
        assert_int_equal(hash_update(&pattern[600], 400), PB_OK);
        assert_int_equal(hash_final(digest, hashes[i].length), PB_OK);
        assert_memory_equal(digest, hashes[i].pattern, hashes[i].length);
    }
}

/* 3.3: the engine hashes a chunk while the next one is fetched */
static void test_3_3_update_async(void **state)
{
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(hashes); i++) {
        unsigned int jobs = caam_model_jobs_done();

        assert_int_equal(hash_init(hashes[i].alg), PB_OK);
        assert_int_equal(hash_update_async(pattern, 512), PB_OK);
        /* It returned before the bytes were hashed */
        assert_int_equal(caam_model_jobs_done(), jobs);
        assert_int_equal(hash_update_async(&pattern[512], 488), PB_OK);
        assert_int_equal(hash_final(digest, hashes[i].length), PB_OK);
        assert_memory_equal(digest, hashes[i].pattern, hashes[i].length);
    }
}

/* 3: between hash_init() and hash_final() the updates may be of any kind
 * and mixed. 3.3: every call waits for the job of the one before it, so that
 * its buffer is the caller's again when the call returns (F-286 (2)) */
static void test_3_3_every_call_waits(void **state)
{
    static uint8_t first[sizeof(chunk)];
    static uint8_t expected[32];
    (void)state;

    /* Two chunks and ten bytes, one job after the other */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(chunk, sizeof(chunk)), PB_OK);
    assert_int_equal(hash_update(pattern, 512), PB_OK);
    assert_int_equal(hash_update(pattern, 10), PB_OK);
    assert_int_equal(hash_final(expected, sizeof(expected)), PB_OK);

    /* A synchronous update after an asynchronous one, and the other way
     * round. The first buffer is reused as soon as the next call returned */
    memcpy(first, chunk, sizeof(first));
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(first, sizeof(first)), PB_OK);
    assert_int_equal(hash_update(pattern, 512), PB_OK);
    memset(first, 0, sizeof(first));
    assert_int_equal(hash_update_async(pattern, 10), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, expected, 32);

    memcpy(first, chunk, sizeof(first));
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(first, sizeof(first)), PB_OK);
    assert_int_equal(hash_update_async(pattern, 512), PB_OK);
    assert_int_equal(hash_update(pattern, 10), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, expected, 32);

    /* A call that only fills the driver's block buffer waits too */
    memcpy(first, chunk, sizeof(first));
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(first, sizeof(first)), PB_OK);
    assert_false(caam_model_idle());
    assert_int_equal(hash_update_async(pattern, 10), PB_OK);
    assert_true(caam_model_idle());
    memset(first, 0, sizeof(first));
    assert_int_equal(hash_update_async(&pattern[10], 502), PB_OK);
    assert_int_equal(hash_update_async(pattern, 10), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, expected, 32);
}

/* 3.3, drivers.md 4.2: the driver's block buffer isn't filled again while
 * the engine reads it. The second update fills the buffer up to a block,
 * which is hashed, and has bytes left for it (F-220) */
static void test_3_3_block_buffer(void **state)
{
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(hashes); i++) {
        assert_int_equal(hash_init(hashes[i].alg), PB_OK);
        assert_int_equal(hash_update_async(pattern, 100), PB_OK);
        assert_int_equal(hash_update_async(&pattern[100], 40), PB_OK);
        assert_int_equal(hash_update_async(&pattern[140], 860), PB_OK);
        assert_int_equal(hash_final(digest, hashes[i].length), PB_OK);
        assert_memory_equal(digest, hashes[i].pattern, hashes[i].length);
    }
}

/* 3.3: an error of the job is returned by the call that waits for it, and
 * the context is then failed: every later update and the final return an
 * error, never a digest */
static void test_3_3_job_error_fails_the_context(void **state)
{
    (void)state;

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    caam_model_fail_next_job(CAAM_STATUS_DECO_WDOG);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_int_not_equal(hash_update_async(pattern, 10), PB_OK);
    assert_int_not_equal(hash_update(pattern, 10), PB_OK);
    assert_int_not_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_int_not_equal(hash_final(digest, 32), PB_OK);
    assert_true(caam_model_idle());

    /* A job that doesn't end fails it too, also when it ends later */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    caam_model_hang(true);
    assert_int_equal(hash_update(pattern, 10), -PB_ERR_TIMEOUT);
    caam_model_hang(false);
    assert_int_equal(hash_update(pattern, 10), -PB_ERR_TIMEOUT);
    assert_int_equal(hash_final(digest, 32), -PB_ERR_TIMEOUT);
    assert_true(caam_model_idle());
}

/* 4: a token verifies with its key, and with one bit changed it doesn't */
static void test_4_verdict(void **state)
{
    (void)state;
    assert_verdicts();
}

/* 3.1: a hash that was given up, at each step, is ended by the next
 * hash_init(): nothing of it completes into the verification that follows
 * (F-181) */
static void test_3_1_hash_init_starts_clean(void **state)
{
    (void)state;

    /* After hash_init() */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_verdicts();

    /* With bytes in the driver's block buffer only */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(chunk, 10), PB_OK);
    assert_verdicts();

    /* After a synchronous update */
    assert_int_equal(hash_init(HASH_SHA512), PB_OK);
    assert_int_equal(hash_update(chunk, sizeof(chunk)), PB_OK);
    assert_verdicts();

    /* With a job in flight: a read error in the boot's load, a failed
     * 'part verify' */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_false(caam_model_idle());
    assert_verdicts();

    /* The same, long after the job was done: its completion is in the ring */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    bench_advance_us(100000);
    assert_verdicts();

    /* And between two chunks */
    assert_int_equal(hash_init(HASH_SHA384), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_verdicts();
}

/* 3.1: the new context has nothing of the old one */
static void test_3_1_hash_init_new_context(void **state)
{
    (void)state;

    assert_int_equal(hash_init(HASH_SHA512), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_int_equal(hash_update_async(chunk, 77), PB_OK);

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(pattern, sizeof(pattern)), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, vector_sha256_pattern, 32);
}

/* 4: the verdict is this call's. A verification starts from an idle engine,
 * also when no hash_init() ended the hash that was given up: here the next
 * one failed, in the core */
static void test_4_verdict_is_this_calls(void **state)
{
    const struct token *t = &tokens[0];
    const struct bpak_key *key = find_key(t->key_id);
    bool verified = true;
    (void)state;

    /* The token's digest, then a hash that is given up with its job in
     * flight */
    assert_int_equal(hash_init(t->hash), PB_OK);
    assert_int_equal(hash_update(VECTOR_TOKEN_UUID, strlen(VECTOR_TOKEN_UUID)), PB_OK);
    assert_int_equal(hash_final(digest, t->digest_length), PB_OK);

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_int_equal(hash_init(HASH_NONE), -PB_ERR_NOT_SUPPORTED);
    assert_false(caam_model_idle());

    memcpy(signature, t->signature, t->signature_length);
    signature[4] ^= 0x80;

    assert_int_not_equal(dsa_verify(t->dsa,
                                    signature,
                                    t->signature_length,
                                    key->data,
                                    key->size,
                                    t->hash,
                                    digest,
                                    t->digest_length,
                                    &verified),
                         PB_OK);
    assert_false(verified);
    assert_true(caam_model_idle());

    /* And with the job's completion long in the ring */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_int_equal(hash_init(HASH_NONE), -PB_ERR_NOT_SUPPORTED);
    bench_advance_us(100000);

    verified = true;
    assert_int_not_equal(dsa_verify(t->dsa,
                                    signature,
                                    t->signature_length,
                                    key->data,
                                    key->size,
                                    t->hash,
                                    digest,
                                    t->digest_length,
                                    &verified),
                         PB_OK);
    assert_false(verified);

    memcpy(signature, t->signature, t->signature_length);
    assert_int_equal(dsa_verify(t->dsa,
                                signature,
                                t->signature_length,
                                key->data,
                                key->size,
                                t->hash,
                                digest,
                                t->digest_length,
                                &verified),
                     PB_OK);
    assert_true(verified);
}

/* drivers.md 3: a job that fails is this call's error, and nothing of it
 * is left for the next one. The status is the output ring entry's (F-127) */
static void test_job_error(void **state)
{
    bool verified = true;
    (void)state;

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    caam_model_fail_next_job(CAAM_STATUS_DECO_WDOG);
    assert_int_not_equal(hash_update(chunk, sizeof(chunk)), PB_OK);
    assert_true(caam_model_idle());
    assert_verdicts();

    /* A verification whose job fails is no verdict */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(VECTOR_TOKEN_UUID, strlen(VECTOR_TOKEN_UUID)), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    memcpy(signature, tokens[0].signature, tokens[0].signature_length);
    caam_model_fail_next_job(CAAM_STATUS_DECO_WDOG);
    assert_int_not_equal(dsa_verify(tokens[0].dsa,
                                    signature,
                                    tokens[0].signature_length,
                                    find_key(tokens[0].key_id)->data,
                                    find_key(tokens[0].key_id)->size,
                                    tokens[0].hash,
                                    digest,
                                    32,
                                    &verified),
                         PB_OK);
    assert_false(verified);
    assert_true(caam_model_idle());
    assert_verdicts();
}

/* A completion that names another descriptor is an error, and it is taken
 * out of the ring: the next job's completion is its own (F-127) */
static void test_job_wrong_descriptor(void **state)
{
    (void)state;

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    caam_model_wrong_address_next();
    assert_int_not_equal(hash_update(chunk, sizeof(chunk)), PB_OK);
    assert_true(caam_model_idle());
    assert_verdicts();
}

/* drivers.md 3: a job that doesn't end is -PB_ERR_TIMEOUT. While it is in
 * flight nothing starts, and when it has ended the next hash is right */
static void test_job_timeout(void **state)
{
    const struct token *t = &tokens[0];
    const struct bpak_key *key = find_key(t->key_id);
    bool verified = true;
    (void)state;

    assert_int_equal(hash_init(t->hash), PB_OK);
    assert_int_equal(hash_update(VECTOR_TOKEN_UUID, strlen(VECTOR_TOKEN_UUID)), PB_OK);
    assert_int_equal(hash_final(digest, t->digest_length), PB_OK);
    memcpy(signature, t->signature, t->signature_length);

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    caam_model_hang(true);
    assert_int_equal(hash_update(chunk, sizeof(chunk)), -PB_ERR_TIMEOUT);

    /* The engine isn't idle: no hash, and no verdict for a good token */
    assert_int_equal(hash_init(HASH_SHA256), -PB_ERR_TIMEOUT);
    assert_int_equal(hash_init(HASH_NONE), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(dsa_verify(t->dsa,
                                signature,
                                t->signature_length,
                                key->data,
                                key->size,
                                t->hash,
                                digest,
                                t->digest_length,
                                &verified),
                     -PB_ERR_TIMEOUT);
    assert_false(verified);

    caam_model_hang(false);
    assert_verdicts();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_3_known_answers, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_3_update_async, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_3_every_call_waits, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_3_block_buffer, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_3_job_error_fails_the_context, setup, teardown),
        cmocka_unit_test_setup_teardown(test_4_verdict, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_1_hash_init_starts_clean, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_1_hash_init_new_context, setup, teardown),
        cmocka_unit_test_setup_teardown(test_4_verdict_is_this_calls, setup, teardown),
        cmocka_unit_test_setup_teardown(test_job_error, setup, teardown),
        cmocka_unit_test_setup_teardown(test_job_wrong_descriptor, setup, teardown),
        cmocka_unit_test_setup_teardown(test_job_timeout, setup, teardown),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
