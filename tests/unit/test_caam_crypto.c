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
#include <stdio.h>
#include <string.h>

extern const struct bpak_keystore keystore_pb;

/* Job ring 0 of the i.MX6UL */
#define CAAM_JR_BASE 0x02141000

/* Where the model cites a hash continued without its running length */
#define SRM_MDHA     "CAAM SRM MDHA"

/* No backend has this one */
#define HASH_NONE    BIT(20)

/* What the engine reads and writes is below 4 GiB (caam_model.h): static */
static uint8_t pattern[VECTOR_PATTERN_LENGTH];
static uint8_t chunk[4096];
static uint8_t digest[CRYPTO_MD_MAX_SZ];
static uint8_t signature[160];

/* The output ring of the stage before pb, and how imx_caam_init() found and
 * left the job ring */
static uint32_t earlier_ring[2];
static unsigned int init_violations;
static bool init_idle;

/* What imx_caam_init() did, and what a pinned test did (test_pinned_*) */
static struct caam_model_event init_events[16];
static unsigned int n_init_events;
static struct caam_model_event events[64];

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
    caam_model_earlier_stage(earlier_ring, 0);
    caam_model_trace(init_events, ARRAY_SIZE(init_events));

    if (imx_caam_init(CAAM_JR_BASE) != PB_OK)
        return -1;

    n_init_events = caam_model_traced();
    caam_model_trace(NULL, 0);
    init_violations = bench_violations();
    init_idle = caam_model_idle();
    return 0;
}

static int setup(void **state)
{
    (void)state;
    bench_reset();
    caam_model_trace(NULL, 0);
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

/* drivers.md 2.1: init starts from a known state. A completion that the
 * stage before left in the job ring is taken out before the ring is set up
 * again, which the engine allows only with no job in it (F-181) */
static void test_2_1_init_empties_the_ring(void **state)
{
    (void)state;
    assert_int_equal(init_violations, 0);
    assert_true(init_idle);
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

/* 3.5: 'length' is the room at digest_out. Less than the digest is
 * -PB_ERR_BUF_TOO_SMALL and nothing is written; with more, the digest's
 * bytes are written and the rest is left. The engine stored the whole digest
 * into a smaller buffer (F-286 (3)) */
static void test_3_5_final_length(void **state)
{
    static uint8_t room[CRYPTO_MD_MAX_SZ + 16];
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(hashes); i++) {
        memset(room, 0xee, sizeof(room));
        assert_int_equal(hash_init(hashes[i].alg), PB_OK);
        assert_int_equal(hash_update("abc", 3), PB_OK);
        assert_int_equal(hash_final(room, hashes[i].length - 1), -PB_ERR_BUF_TOO_SMALL);
        assert_true(caam_model_idle());
        for (size_t n = 0; n < sizeof(room); n++)
            assert_int_equal(room[n], 0xee);

        /* The context is closed whatever final returned */
        assert_int_equal(hash_update("abc", 3), -PB_ERR_STATE);

        assert_int_equal(hash_init(hashes[i].alg), PB_OK);
        assert_int_equal(hash_update("abc", 3), PB_OK);
        assert_int_equal(hash_final(room, sizeof(room)), PB_OK);
        assert_memory_equal(room, hashes[i].abc, hashes[i].length);
        for (size_t n = hashes[i].length; n < sizeof(room); n++)
            assert_int_equal(room[n], 0xee);
    }
}

/* 3.5: the digest reaches digest_out by a copy, not by the engine's own
 * write. A final whose job timed out and ended later wrote the caller's
 * buffer, long after hash_final() returned (F-081, F-286 (3)) */
static void test_3_5_the_engine_doesnt_write_the_callers_buffer(void **state)
{
    static uint8_t room[CRYPTO_MD_MAX_SZ];
    (void)state;

    memset(room, 0xee, sizeof(room));
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(pattern, 64), PB_OK);
    caam_model_hang(true);
    assert_int_equal(hash_final(room, sizeof(room)), -PB_ERR_TIMEOUT);
    caam_model_hang(false);
    assert_false(caam_model_idle());
    bench_advance_us(100000);
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_true(caam_model_idle());

    for (size_t n = 0; n < sizeof(room); n++)
        assert_int_equal(room[n], 0xee);
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

/* 4: a signature that doesn't verify is -PB_ERR_SIGNATURE, everything else
 * is the code of what failed. It was -PB_ERR, as an engine's error was
 * (F-286 (4)) */
static void test_4_codes(void **state)
{
    static uint8_t key[256];
    const struct token *t = &tokens[0];
    const struct bpak_key *p256 = find_key(t->key_id);
    bool verified = true;
    (void)state;

    /* One bit of the signature, of the digest */
    assert_int_equal(verify_token(t, false, &verified), -PB_ERR_SIGNATURE);
    assert_false(verified);

    assert_int_equal(verify_token(t, true, &verified), PB_OK);
    digest[0] ^= 1;
    assert_int_equal(dsa_verify(t->dsa,
                                signature,
                                t->signature_length,
                                p256->data,
                                p256->size,
                                t->hash,
                                digest,
                                t->digest_length,
                                &verified),
                     -PB_ERR_SIGNATURE);
    assert_false(verified);
    digest[0] ^= 1;

    /* P-521's signature, larger than P-256's order */
    assert_int_equal(dsa_verify(t->dsa,
                                vector_token_p521,
                                sizeof(vector_token_p521),
                                p256->data,
                                p256->size,
                                t->hash,
                                digest,
                                t->digest_length,
                                &verified),
                     -PB_ERR_SIGNATURE);

    /* A signature that can't be parsed */
    signature[0] = 0x31;
    assert_int_equal(dsa_verify(t->dsa,
                                signature,
                                t->signature_length,
                                p256->data,
                                p256->size,
                                t->hash,
                                digest,
                                t->digest_length,
                                &verified),
                     -PB_ERR_ASN1);
    signature[0] = 0x30;

    /* A key that can't be parsed, a key of a curve no backend has: the last
     * byte of the curve's OID is at 22 */
    memcpy(key, p256->data, p256->size);
    key[0] = 0x31;
    assert_int_equal(dsa_verify(t->dsa,
                                signature,
                                t->signature_length,
                                key,
                                p256->size,
                                t->hash,
                                digest,
                                t->digest_length,
                                &verified),
                     -PB_ERR_ASN1);
    memcpy(key, p256->data, p256->size);
    assert_int_equal(key[22], 0x07);
    key[22] = 0x08;
    assert_int_equal(dsa_verify(t->dsa,
                                signature,
                                t->signature_length,
                                key,
                                p256->size,
                                t->hash,
                                digest,
                                t->digest_length,
                                &verified),
                     -PB_ERR_NOT_SUPPORTED);

    /* An error of the engine */
    caam_model_fail_next_job(CAAM_STATUS_DECO_WDOG);
    assert_int_equal(dsa_verify(t->dsa,
                                signature,
                                t->signature_length,
                                p256->data,
                                p256->size,
                                t->hash,
                                digest,
                                t->digest_length,
                                &verified),
                     -PB_ERR_IO);
    assert_false(verified);
}

/* drivers.md 4.2: an address the engine can't take is refused, not cut. The
 * descriptors hold 32 bits: a buffer above 4 GiB was hashed, and a digest
 * verified, at the address cut to 32 bits (F-410). Here that is the test's
 * own buffer, so that the cut address reads the same bytes */
static void test_an_address_above_4gib_is_refused(void **state)
{
    const struct token *t = &tokens[0];
    const struct bpak_key *key = find_key(t->key_id);
    bool verified = true;
    (void)state;

    if (sizeof(uintptr_t) < 8)
        skip();

    const uintptr_t above = (uintptr_t)1 << 32;

    /* The caller's blocks, which the engine reads where they are */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update((const void *)((uintptr_t)chunk + above), sizeof(chunk)),
                     -PB_ERR_PARAM);
    /* The hash is failed: no digest without those bytes */
    assert_int_not_equal(hash_update(pattern, 64), PB_OK);
    assert_int_not_equal(hash_final(digest, 32), PB_OK);

    assert_int_equal(verify_token(t, true, &verified), PB_OK);
    assert_int_equal(dsa_verify(t->dsa,
                                signature,
                                t->signature_length,
                                key->data,
                                key->size,
                                t->hash,
                                (uint8_t *)((uintptr_t)digest + above),
                                t->digest_length,
                                &verified),
                     -PB_ERR_PARAM);
    assert_false(verified);
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

    /* A signature that parses and doesn't verify: a job is added for it */
    memcpy(signature, t->signature, t->signature_length);
    signature[t->signature_length - 1] ^= 1;

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

/* 4, drivers.md 5: the completion a call takes is its own job's. One of
 * another descriptor, which an earlier user of the job ring left, is taken
 * out of the ring and the job is waited for: it is still in flight. Taking
 * it for the job's end left every later wait with the completion of the job
 * before its own, and a signature that doesn't verify was accepted (F-181) */
static void test_4_completion_of_another_job(void **state)
{
    static uint8_t expected[32];
    (void)state;

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(chunk, sizeof(chunk)), PB_OK);
    assert_int_equal(hash_final(expected, sizeof(expected)), PB_OK);

    /* In the ring before the job is added */
    caam_model_foreign_completion(0);
    assert_false(caam_model_idle());
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(chunk, sizeof(chunk)), PB_OK);
    assert_true(caam_model_idle());
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, expected, 32);

    /* With a status that isn't the job's, before a verification */
    caam_model_foreign_completion(CAAM_STATUS_DECO_WDOG);
    assert_verdicts();

    /* Before the job of an asynchronous update that is given up */
    caam_model_foreign_completion(0);
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    assert_verdicts();
}

/* The status of a job is its output ring entry's (F-127). JRSTAR has the
 * status of the completion that was written last: here that of another
 * job's, which ended behind the hash's and with an error, by the time the
 * hash's entry is taken out */
static void test_job_status_is_the_entrys(void **state)
{
    static uint8_t expected[32];
    (void)state;

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update(chunk, sizeof(chunk)), PB_OK);
    assert_int_equal(hash_update(pattern, 10), PB_OK);
    assert_int_equal(hash_final(expected, sizeof(expected)), PB_OK);

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update_async(chunk, sizeof(chunk)), PB_OK);
    caam_model_foreign_completion(CAAM_STATUS_DECO_WDOG);
    assert_int_equal(hash_update(pattern, 10), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);
    assert_memory_equal(digest, expected, 32);
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

/*
 * The pinned tests: the register writes and the descriptors of an init, of
 * hashes and of verifications that succeed, word by word, as the engine took
 * them. The CAAM signs off every boot on the boards that have it, and none
 * is attached to the tests: a change that isn't meant to reach the engine
 * must leave these as they are. An address is named, not given as a number:
 * the test's buffers by their name and offset, the driver's own memory as
 * drvN, numbered as it first appears.
 */
static const struct {
    const void *buf;
    size_t len;
    const char *name;
} named_buffers[] = {
    { pattern, sizeof(pattern), "pattern" },
    { chunk, sizeof(chunk), "chunk" },
    { digest, sizeof(digest), "digest" },
    { signature, sizeof(signature), "signature" },
};

static uint32_t driver_addresses[16];
static unsigned int n_driver_addresses;

static void name_address(char *out, size_t size, uint32_t addr)
{
    if (addr == 0) {
        snprintf(out, size, "NULL");
        return;
    }

    for (size_t i = 0; i < ARRAY_SIZE(named_buffers); i++) {
        uintptr_t start = (uintptr_t)named_buffers[i].buf;

        if (addr >= start && addr < start + named_buffers[i].len) {
            if (addr == start)
                snprintf(out, size, "%s", named_buffers[i].name);
            else
                snprintf(out, size, "%s+%lu", named_buffers[i].name, (unsigned long)(addr - start));
            return;
        }
    }

    for (unsigned int i = 0; i < n_driver_addresses; i++) {
        if (driver_addresses[i] == addr) {
            snprintf(out, size, "drv%u", i);
            return;
        }
    }

    assert_true(n_driver_addresses < ARRAY_SIZE(driver_addresses));
    driver_addresses[n_driver_addresses] = addr;
    snprintf(out, size, "drv%u", n_driver_addresses++);
}

static const char *register_name(uint32_t reg)
{
    switch (reg) {
    case 0x004:
        return "IRBAR_LS";
    case 0x00c:
        return "IRSR";
    case 0x01c:
        return "IRJAR";
    case 0x024:
        return "ORBAR_LS";
    case 0x02c:
        return "ORSR";
    case 0x034:
        return "ORJRR";
    default:
        return "?";
    }
}

static void format_event(char *out, size_t size, const struct caam_model_event *e)
{
    char name[32];
    size_t n;

    if (!e->job) {
        if (e->addresses & 1)
            name_address(name, sizeof(name), e->value);
        else
            snprintf(name, sizeof(name), "%u", e->value);
        snprintf(out, size, "%s = %s", register_name(e->reg), name);
        return;
    }

    name_address(name, sizeof(name), e->value);
    n = (size_t)snprintf(out, size, "job %s:", name);

    for (unsigned int i = 0; i < e->len && n < size; i++) {
        if (e->addresses & (1ull << i))
            name_address(name, sizeof(name), e->words[i]);
        else
            snprintf(name, sizeof(name), "%08x", e->words[i]);
        n += (size_t)snprintf(&out[n], size - n, " %s", name);
    }
}

/* Every event against its line of 'expected'. On a difference the whole
 * trace is printed, ready to paste */
static void assert_events(const struct caam_model_event *e,
                          unsigned int n,
                          const char *const *expected,
                          size_t n_expected)
{
    static char lines[64][640];
    bool same = (n == n_expected);

    assert_true(n <= ARRAY_SIZE(lines));

    for (unsigned int i = 0; i < n; i++) {
        format_event(lines[i], sizeof(lines[i]), &e[i]);
        if (i < n_expected && strcmp(lines[i], expected[i]) != 0)
            same = false;
    }

    if (!same) {
        for (unsigned int i = 0; i < n; i++)
            print_message("        \"%s\",\n", lines[i]);
        fail_msg("The engine's trace differs from the pinned one (%u events, %zu pinned)",
                 n,
                 n_expected);
    }
}

/* The addresses are named in the order they appear, init's first */
static void pin_begin(void)
{
    /* The completion the stage before left is taken out, then the rings of
     * one entry each are set up */
    static const char *const expected[] = {
        "ORJRR = 1", "IRBAR_LS = drv0", "ORBAR_LS = drv1", "IRSR = 1", "ORSR = 1",
    };

    n_driver_addresses = 0;
    assert_events(init_events, n_init_events, expected, ARRAY_SIZE(expected));
    caam_model_trace(events, ARRAY_SIZE(events));
}

static void test_pinned_init_and_hashes(void **state)
{
    static const char *const expected[] = {
        "IRJAR = 1",
        "job drv2: b0800007 84430004 24540000 NULL 00000000 54200028 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84430008 14200028 drv3 24540000 drv4 00000003 54200020 drv5",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800007 84450004 24540000 drv4 00000080 54200048 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84450000 14200048 drv3 24540000 pattern+128 00000180 54200048 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84450000 14200048 drv3 24540000 drv4 00000080 54200048 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84450000 14200048 drv3 24540000 pattern+640 00000100 54200048 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84450008 14200048 drv3 24540000 drv4 00000068 54200040 drv5",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800007 84440004 24540000 pattern 00000200 54200048 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84440000 14200048 drv3 24540000 pattern+512 00000180 54200048 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84440008 14200048 drv3 24540000 drv4 00000068 54200030 drv5",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800007 84400004 24540000 chunk 000000c0 54200018 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84400008 14200018 drv3 24540000 drv4 00000008 54200010 drv5",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800007 84400004 24540000 chunk 000000c0 54200010 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84400008 14200010 drv3 24540000 drv4 00000008 54200010 drv5",
        "ORJRR = 1",
    };
    (void)state;

    pin_begin();

    /* Less than a block: a job that starts the hash with no data, then the
     * final with the block buffer */
    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_update("abc", 3), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);

    /* Blocks from the block buffer and from the caller's buffer */
    assert_int_equal(hash_init(HASH_SHA512), PB_OK);
    assert_int_equal(hash_update(pattern, 100), PB_OK);
    assert_int_equal(hash_update(&pattern[100], 500), PB_OK);
    assert_int_equal(hash_update(&pattern[600], 400), PB_OK);
    assert_int_equal(hash_final(digest, 64), PB_OK);

    /* Asynchronous updates */
    assert_int_equal(hash_init(HASH_SHA384), PB_OK);
    assert_int_equal(hash_update_async(pattern, 512), PB_OK);
    assert_int_equal(hash_update_async(&pattern[512], 488), PB_OK);
    assert_int_equal(hash_final(digest, 48), PB_OK);

    assert_int_equal(hash_init(HASH_MD5), PB_OK);
    assert_int_equal(hash_update(chunk, 200), PB_OK);
    assert_int_equal(hash_final(digest, 16), PB_OK);

    /* The broken MD5's running context has no message length, which the
     * model logs as a violation. Its digest isn't known, its words are */
    assert_int_equal(hash_init(HASH_MD5_BROKEN), PB_OK);
    assert_int_equal(hash_update(chunk, 200), PB_OK);
    assert_int_equal(hash_final(digest, 16), PB_OK);
    assert_int_equal(bench_violations(), 1);
    assert_int_equal(bench_violations_of(SRM_MDHA), 1);
    bench_reset();

    assert_events(events, caam_model_traced(), expected, ARRAY_SIZE(expected));
    caam_model_trace(NULL, 0);
}

static void test_pinned_verifications(void **state)
{
    static const char *const expected[] = {
        "IRJAR = 1",
        "job drv2: b0800007 84430004 24540000 NULL 00000000 54200028 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84430008 14200028 drv3 24540000 drv4 00000024 54200020 drv5",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0880009 00400100 drv6 digest drv7 drv8 drv9 00000020 80160802",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800007 84440004 24540000 NULL 00000000 54200048 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84440008 14200048 drv3 24540000 drv4 00000024 54200030 drv5",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0880009 00400180 drv6 digest drv7 drv8 drv9 00000030 80160802",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800007 84450004 24540000 NULL 00000000 54200048 drv3",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0800009 84450008 14200048 drv3 24540000 drv4 00000024 54200040 drv5",
        "ORJRR = 1",
        "IRJAR = 1",
        "job drv2: b0880009 00400200 drv6 digest drv7 drv8 drv9 00000040 80160802",
        "ORJRR = 1",
    };
    (void)state;

    pin_begin();

    for (size_t i = 0; i < ARRAY_SIZE(tokens); i++) {
        bool verified = false;

        assert_int_equal(verify_token(&tokens[i], true, &verified), PB_OK);
        assert_true(verified);
    }

    assert_events(events, caam_model_traced(), expected, ARRAY_SIZE(expected));
    caam_model_trace(NULL, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_2_1_init_empties_the_ring, setup, teardown),
        cmocka_unit_test_setup_teardown(test_pinned_init_and_hashes, setup, teardown),
        cmocka_unit_test_setup_teardown(test_pinned_verifications, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_known_answers, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_3_update_async, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_5_final_length, setup, teardown),
        cmocka_unit_test_setup_teardown(
            test_3_5_the_engine_doesnt_write_the_callers_buffer, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_3_every_call_waits, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_3_block_buffer, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_3_job_error_fails_the_context, setup, teardown),
        cmocka_unit_test_setup_teardown(test_4_verdict, setup, teardown),
        cmocka_unit_test_setup_teardown(test_4_codes, setup, teardown),
        cmocka_unit_test_setup_teardown(test_an_address_above_4gib_is_refused, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_1_hash_init_starts_clean, setup, teardown),
        cmocka_unit_test_setup_teardown(test_3_1_hash_init_new_context, setup, teardown),
        cmocka_unit_test_setup_teardown(test_4_verdict_is_this_calls, setup, teardown),
        cmocka_unit_test_setup_teardown(test_job_error, setup, teardown),
        cmocka_unit_test_setup_teardown(test_4_completion_of_another_job, setup, teardown),
        cmocka_unit_test_setup_teardown(test_job_status_is_the_entrys, setup, teardown),
        cmocka_unit_test_setup_teardown(test_job_timeout, setup, teardown),
    };

    return cmocka_run_group_tests(tests, group_setup, NULL);
}
