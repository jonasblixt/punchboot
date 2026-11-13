/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * See crypto_conformance.h.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <bpak/keystore.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <string.h>

#include "crypto_conformance.h"
#include "crypto_vectors.h"

extern const struct bpak_keystore keystore_pb;

hash_t crypto_conformance_hashes;
dsa_t crypto_conformance_curves;

#define GUARD 0xee

/* Static, and on a cache line: an engine's model takes 32 bit addresses */
static uint8_t pattern[VECTOR_PATTERN_LENGTH] __aligned(64);
static uint8_t digest[CRYPTO_MD_MAX_SZ] __aligned(64);
static uint8_t room[CRYPTO_MD_MAX_SZ + 16] __aligned(64);
static uint8_t signature[160] __aligned(64);
static uint8_t key[256] __aligned(64);

static const struct hash_case {
    hash_t alg;
    size_t length;
    const uint8_t *abc;
    const uint8_t *pattern;
    size_t prefix; /* The digest's offset in struct vector_prefix */
} hash_cases[] = {
    { HASH_MD5, 16, vector_md5_abc, vector_md5_pattern, offsetof(struct vector_prefix, md5) },
    { HASH_SHA256,
      32,
      vector_sha256_abc,
      vector_sha256_pattern,
      offsetof(struct vector_prefix, sha256) },
    { HASH_SHA384,
      48,
      vector_sha384_abc,
      vector_sha384_pattern,
      offsetof(struct vector_prefix, sha384) },
    { HASH_SHA512,
      64,
      vector_sha512_abc,
      vector_sha512_pattern,
      offsetof(struct vector_prefix, sha512) },
};

/* Command mode's token, per curve: the hash that goes with the curve, over
 * VECTOR_TOKEN_UUID, signed with the internal keystore's key */
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

static void begin(void)
{
    for (size_t i = 0; i < sizeof(pattern); i++)
        pattern[i] = (uint8_t)(i * 7 + 3);
}

static bool has_hash(hash_t alg)
{
    return (crypto_conformance_hashes & alg) != 0;
}

/* The token can be checked: the backend has its curve and its hash */
static bool has_token(const struct token *t)
{
    return (crypto_conformance_curves & t->dsa) && has_hash(t->hash);
}

static const struct token *token_of(dsa_t dsa)
{
    for (size_t i = 0; i < ARRAY_SIZE(tokens); i++) {
        if (tokens[i].dsa == dsa)
            return has_token(&tokens[i]) ? &tokens[i] : NULL;
    }
    return NULL;
}

static const struct bpak_key *key_of(const struct token *t)
{
    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id == t->key_id)
            return keystore_pb.keys[i];
    }
    fail_msg("No key 0x%x in the keystore", t->key_id);
    return NULL;
}

static void assert_digest(const struct hash_case *h, const uint8_t *expected)
{
    assert_int_equal(hash_final(digest, h->length), PB_OK);
    assert_memory_equal(digest, expected, h->length);
}

/* The token's digest into 'digest', and its signature into 'signature' */
static void token_begin(const struct token *t)
{
    assert_int_equal(hash_init(t->hash), PB_OK);
    assert_int_equal(hash_update(VECTOR_TOKEN_UUID, strlen(VECTOR_TOKEN_UUID)), PB_OK);
    assert_int_equal(hash_final(digest, t->digest_length), PB_OK);
    memcpy(signature, t->signature, t->signature_length);
}

static int verify(const struct token *t,
                  size_t signature_length,
                  const uint8_t *der_key,
                  size_t key_length,
                  bool *verified)
{
    int rc;

    *verified = true;
    rc = dsa_verify(t->dsa,
                    signature,
                    signature_length,
                    der_key,
                    key_length,
                    t->hash,
                    digest,
                    t->digest_length,
                    verified);

    /* crypto.md 4: PB_OK if and only if *verified */
    assert_int_equal(rc == PB_OK, *verified);
    return rc;
}

static int verify_token(const struct token *t, bool *verified)
{
    const struct bpak_key *k = key_of(t);

    return verify(t, t->signature_length, k->data, k->size, verified);
}

/* crypto.md 3: the standards' digests, of "abc", of the pattern, of the
 * pattern's first bytes around the block sizes, of no bytes, also with an
 * update of no bytes */
void crypto_3_known_answers(void **state)
{
    (void)state;
    begin();

    for (size_t i = 0; i < ARRAY_SIZE(hash_cases); i++) {
        const struct hash_case *h = &hash_cases[i];

        if (!has_hash(h->alg))
            continue;

        assert_int_equal(hash_init(h->alg), PB_OK);
        assert_int_equal(hash_update("abc", 3), PB_OK);
        assert_digest(h, h->abc);

        assert_int_equal(hash_init(h->alg), PB_OK);
        assert_int_equal(hash_update(pattern, sizeof(pattern)), PB_OK);
        assert_digest(h, h->pattern);

        for (size_t n = 0; n < ARRAY_SIZE(vector_prefixes); n++) {
            const struct vector_prefix *v = &vector_prefixes[n];
            const uint8_t *expected = (const uint8_t *)v + h->prefix;

            assert_int_equal(hash_init(h->alg), PB_OK);
            if (v->length > 0)
                assert_int_equal(hash_update(pattern, v->length), PB_OK);
            assert_digest(h, expected);

            /* The same with an update of no bytes on either side */
            assert_int_equal(hash_init(h->alg), PB_OK);
            assert_int_equal(hash_update(pattern, 0), PB_OK);
            assert_int_equal(hash_update_async(pattern, v->length), PB_OK);
            assert_int_equal(hash_update(pattern, 0), PB_OK);
            assert_digest(h, expected);
        }
    }
}

/* crypto.md 3.2, 3.3: the digest is that of the bytes in the order of the
 * calls, however they were split, with updates of any kind, mixed */
void crypto_3_every_split(void **state)
{
    (void)state;
    begin();

    for (size_t i = 0; i < ARRAY_SIZE(hash_cases); i++) {
        const struct hash_case *h = &hash_cases[i];

        if (!has_hash(h->alg))
            continue;

        for (size_t k = 0; k <= sizeof(pattern); k++) {
            assert_int_equal(hash_init(h->alg), PB_OK);
            assert_int_equal(hash_update(pattern, k), PB_OK);
            assert_int_equal(hash_update(&pattern[k], sizeof(pattern) - k), PB_OK);
            assert_digest(h, h->pattern);

            /* Three, asynchronous around a synchronous one */
            size_t m = MIN((size_t)77, sizeof(pattern) - k);

            assert_int_equal(hash_init(h->alg), PB_OK);
            assert_int_equal(hash_update_async(pattern, k), PB_OK);
            assert_int_equal(hash_update(&pattern[k], m), PB_OK);
            assert_int_equal(hash_update_async(&pattern[k + m], sizeof(pattern) - k - m), PB_OK);
            assert_digest(h, h->pattern);
        }
    }
}

/* crypto.md 2, 3.1: an algorithm the backend doesn't register is
 * -PB_ERR_NOT_SUPPORTED, and there is no context; not one algorithm is
 * -PB_ERR_PARAM */
void crypto_3_1_an_algorithm_the_backend_lacks(void **state)
{
    static const hash_t all[] = { HASH_MD5, HASH_MD5_BROKEN, HASH_SHA256, HASH_SHA384, HASH_SHA512 };
    bool verified = true;
    (void)state;
    begin();

    for (size_t i = 0; i < ARRAY_SIZE(all); i++) {
        if (has_hash(all[i]))
            continue;
        assert_int_equal(hash_init(all[i]), -PB_ERR_NOT_SUPPORTED);
        assert_int_equal(hash_update(pattern, 3), -PB_ERR_STATE);
    }

    assert_int_equal(hash_init(0), -PB_ERR_PARAM);
    assert_int_equal(hash_init(HASH_SHA256 | HASH_SHA512), -PB_ERR_PARAM);
    assert_int_equal(hash_final(digest, sizeof(digest)), -PB_ERR_STATE);

    for (size_t i = 0; i < ARRAY_SIZE(tokens); i++) {
        if (crypto_conformance_curves & tokens[i].dsa)
            continue;
        assert_int_equal(dsa_verify(tokens[i].dsa,
                                    tokens[i].signature,
                                    tokens[i].signature_length,
                                    key,
                                    sizeof(key),
                                    tokens[i].hash,
                                    digest,
                                    tokens[i].digest_length,
                                    &verified),
                         -PB_ERR_NOT_SUPPORTED);
        assert_false(verified);
    }
}

/* crypto.md 3.5: less room than the digest is -PB_ERR_BUF_TOO_SMALL and
 * nothing is written; with more the digest's bytes are written and the rest
 * is left. The context is closed whatever final returns */
void crypto_3_5_final_writes_its_room(void **state)
{
    (void)state;
    begin();

    for (size_t i = 0; i < ARRAY_SIZE(hash_cases); i++) {
        const struct hash_case *h = &hash_cases[i];

        if (!has_hash(h->alg))
            continue;

        memset(room, GUARD, sizeof(room));
        assert_int_equal(hash_init(h->alg), PB_OK);
        assert_int_equal(hash_update("abc", 3), PB_OK);
        assert_int_equal(hash_final(room, h->length - 1), -PB_ERR_BUF_TOO_SMALL);
        for (size_t n = 0; n < sizeof(room); n++)
            assert_int_equal(room[n], GUARD);
        assert_int_equal(hash_update("abc", 3), -PB_ERR_STATE);
        assert_int_equal(hash_final(room, sizeof(room)), -PB_ERR_STATE);

        assert_int_equal(hash_init(h->alg), PB_OK);
        assert_int_equal(hash_update_async(pattern, sizeof(pattern)), PB_OK);
        assert_int_equal(hash_final(room, sizeof(room)), PB_OK);
        assert_memory_equal(room, h->pattern, h->length);
        for (size_t n = h->length; n < sizeof(room); n++)
            assert_int_equal(room[n], GUARD);
    }
}

/* crypto.md 3: a call without a context is -PB_ERR_STATE: before the first
 * hash_init() of the test and after a final */
void crypto_3_no_context(void **state)
{
    (void)state;
    begin();

    assert_int_equal(hash_init(HASH_SHA256), PB_OK);
    assert_int_equal(hash_final(digest, 32), PB_OK);

    assert_int_equal(hash_update(pattern, 1), -PB_ERR_STATE);
    assert_int_equal(hash_update_async(pattern, 1), -PB_ERR_STATE);
    assert_int_equal(hash_copy_update(pattern, room, 1), -PB_ERR_STATE);
    assert_int_equal(hash_final(digest, 32), -PB_ERR_STATE);
}

/* crypto.md 4: a valid signature verifies, with its key, on each curve */
void crypto_4_a_token_verifies(void **state)
{
    bool verified;
    (void)state;
    begin();

    for (size_t i = 0; i < ARRAY_SIZE(tokens); i++) {
        if (!has_token(&tokens[i]))
            continue;
        token_begin(&tokens[i]);
        assert_int_equal(verify_token(&tokens[i], &verified), PB_OK);
        assert_true(verified);
    }
}

/* crypto.md 4: one bit changed in the signature or in the digest is a
 * signature that doesn't verify, -PB_ERR_SIGNATURE. One in the key's point
 * is no "yes": a backend may find that the point isn't on its curve
 * (-PB_ERR_ASN1) or that the signature doesn't verify */
void crypto_4_one_bit_changed(void **state)
{
    bool verified;
    int rc;
    (void)state;
    begin();

    for (size_t i = 0; i < ARRAY_SIZE(tokens); i++) {
        const struct token *t = &tokens[i];
        const struct bpak_key *k;

        if (!has_token(t))
            continue;

        k = key_of(t);

        token_begin(t);
        signature[t->signature_length - 1] ^= 1;
        assert_int_equal(verify_token(t, &verified), -PB_ERR_SIGNATURE);

        token_begin(t);
        digest[t->digest_length - 1] ^= 1;
        assert_int_equal(verify_token(t, &verified), -PB_ERR_SIGNATURE);

        token_begin(t);
        memcpy(key, k->data, k->size);
        key[k->size - 1] ^= 1;
        rc = verify(t, t->signature_length, key, k->size, &verified);
        assert_false(verified);
        if (rc != -PB_ERR_SIGNATURE && rc != -PB_ERR_ASN1)
            fail_msg("A key with a bit changed: %i", rc);

        /* And the token still verifies */
        token_begin(t);
        assert_int_equal(verify_token(t, &verified), PB_OK);
    }
}

/* crypto.md 4: a signature or a key that can't be parsed is -PB_ERR_ASN1, a
 * key of a curve no backend has -PB_ERR_NOT_SUPPORTED */
void crypto_4_malformed_der(void **state)
{
    bool verified;
    (void)state;
    begin();

    for (size_t i = 0; i < ARRAY_SIZE(tokens); i++) {
        const struct token *t = &tokens[i];
        const struct bpak_key *k;

        if (!has_token(t))
            continue;

        k = key_of(t);

        /* Not a SEQUENCE, an INTEGER that isn't one, cut short */
        token_begin(t);
        signature[0] = 0x31;
        assert_int_equal(verify_token(t, &verified), -PB_ERR_ASN1);
        token_begin(t);
        signature[(t->signature[1] & 0x80) ? 3 : 2] = 0x03;
        assert_int_equal(verify_token(t, &verified), -PB_ERR_ASN1);
        token_begin(t);
        assert_int_equal(verify(t, t->signature_length - 1, k->data, k->size, &verified),
                         -PB_ERR_ASN1);

        /* The key: not a SEQUENCE, cut short */
        memcpy(key, k->data, k->size);
        key[0] = 0x31;
        assert_int_equal(verify(t, t->signature_length, key, k->size, &verified), -PB_ERR_ASN1);
        assert_int_equal(verify(t, t->signature_length, k->data, k->size - 1, &verified),
                         -PB_ERR_ASN1);
    }

    /* P-256's key with a curve OID that names no curve: its last byte is
     * at 22 */
    const struct token *t = token_of(DSA_EC_SECP256r1);

    if (t != NULL) {
        const struct bpak_key *k = key_of(t);

        token_begin(t);
        memcpy(key, k->data, k->size);
        assert_int_equal(key[22], 0x07);
        key[22] = 0x08;
        assert_int_equal(verify(t, t->signature_length, key, k->size, &verified),
                         -PB_ERR_NOT_SUPPORTED);
    }
}

/* crypto.md 4: a signature made with another curve's key doesn't verify,
 * also one whose r and s are larger than the key's curve */
void crypto_4_another_curves_signature(void **state)
{
    const struct token *p256 = token_of(DSA_EC_SECP256r1);
    const struct token *p384 = token_of(DSA_EC_SECP384r1);
    const struct token *p521 = token_of(DSA_EC_SECP521r1);
    bool verified;
    (void)state;
    begin();

    if (p256 == NULL || p384 == NULL || p521 == NULL)
        skip();

    /* P-256's token, checked with P-384's key */
    token_begin(p256);
    assert_int_equal(dsa_verify(DSA_EC_SECP384r1,
                                signature,
                                p256->signature_length,
                                key_of(p384)->data,
                                key_of(p384)->size,
                                p256->hash,
                                digest,
                                p256->digest_length,
                                &verified),
                     -PB_ERR_SIGNATURE);
    assert_false(verified);

    /* P-521's signature, with P-256's key and P-256's token's digest */
    memcpy(signature, p521->signature, p521->signature_length);
    assert_int_equal(
        verify(p256, p521->signature_length, key_of(p256)->data, key_of(p256)->size, &verified),
        -PB_ERR_SIGNATURE);
}

/* crypto.md 3.1, 4 and 8: a hash given up at each step. A verification is
 * refused while it is open; the next hash_init() ends it, and then a
 * verification that must fail fails and one that must pass passes */
void crypto_4_a_hash_given_up_at_each_step(void **state)
{
    const struct token *t = token_of(DSA_EC_SECP256r1);
    bool verified;
    (void)state;
    begin();

    if (t == NULL)
        skip();

    for (unsigned int step = 0; step < 6; step++) {
        token_begin(t);

        assert_int_equal(hash_init(HASH_SHA256), PB_OK);
        switch (step) {
        case 0: /* Right after hash_init() */
            break;
        case 1: /* Less than a block */
            assert_int_equal(hash_update(pattern, 10), PB_OK);
            break;
        case 2: /* Whole blocks */
            assert_int_equal(hash_update(pattern, 512), PB_OK);
            break;
        case 3: /* An asynchronous update, maybe in flight */
            assert_int_equal(hash_update_async(pattern, sizeof(pattern)), PB_OK);
            break;
        case 4: /* Two of them */
            assert_int_equal(hash_update_async(pattern, 512), PB_OK);
            assert_int_equal(hash_update_async(&pattern[512], 488), PB_OK);
            break;
        default: /* A final that failed closes it */
            assert_int_equal(hash_update_async(pattern, sizeof(pattern)), PB_OK);
            assert_int_equal(hash_final(room, 31), -PB_ERR_BUF_TOO_SMALL);
            break;
        }

        if (step < 5)
            assert_int_equal(verify_token(t, &verified), -PB_ERR_STATE);

        /* The verification as its users make it */
        token_begin(t);
        signature[t->signature_length - 1] ^= 1;
        assert_int_equal(verify_token(t, &verified), -PB_ERR_SIGNATURE);
        signature[t->signature_length - 1] ^= 1;
        assert_int_equal(verify_token(t, &verified), PB_OK);
    }
}
