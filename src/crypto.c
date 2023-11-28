/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <inttypes.h>
#include <pb/crypto.h>
#include <pb/pb.h>
#include <pb/self_test.h>
#include <string.h>

static const struct dsa_ops *dsa_ops[CONFIG_CRYPTO_MAX_DSA_OPS];
static size_t no_of_dsa_ops;
static const struct hash_ops *hash_ops[CONFIG_CRYPTO_MAX_HASH_OPS];
static size_t no_of_hash_ops;
static const struct hash_ops *current_hash_ops; /* Currently active context */

int hash_init(hash_t alg)
{
    current_hash_ops = NULL;

    for (int i = 0; i < CONFIG_CRYPTO_MAX_HASH_OPS; i++) {
        if (hash_ops[i] && (hash_ops[i]->alg_bits & alg)) {
            current_hash_ops = hash_ops[i];
            return current_hash_ops->init(alg);
        }
    }

    return PB_ERR_NOT_SUPPORTED;
}

int hash_update(const void *buf, size_t length)
{
    if (current_hash_ops == NULL)
        return -PB_ERR_STATE;

    return current_hash_ops->update(buf, length);
}

int hash_update_async(const void *buf, size_t length)
{
    if (current_hash_ops == NULL)
        return -PB_ERR_STATE;

    if (current_hash_ops->update_async)
        return current_hash_ops->update_async(buf, length);
    else
        return current_hash_ops->update(buf, length);
}

int hash_copy_update(const void *src, void *dest, size_t length)
{
    if (current_hash_ops == NULL)
        return -PB_ERR_STATE;

    if (current_hash_ops->copy_update != NULL) {
        return current_hash_ops->copy_update(src, dest, length);
    } else {
        memcpy(dest, src, length);
        return current_hash_ops->update(dest, length);
    }
}

int hash_final(uint8_t *digest_output, size_t length)
{
    if (current_hash_ops == NULL)
        return -PB_ERR_STATE;
    int rc = current_hash_ops->final(digest_output, length);
    current_hash_ops = NULL;
    return rc;
}

int hash_add_ops(const struct hash_ops *ops)
{
    if (no_of_hash_ops >= CONFIG_CRYPTO_MAX_HASH_OPS)
        return -PB_ERR_MEM;

    LOG_INFO("Register hash ops: %s, 0x%x", ops->name, ops->alg_bits);
    hash_ops[no_of_hash_ops++] = ops;

    return PB_OK;
}

int dsa_verify(dsa_t alg,
               const uint8_t *der_signature,
               size_t signature_length,
               const uint8_t *der_key,
               size_t key_length,
               hash_t md_alg,
               uint8_t *md,
               size_t md_length,
               bool *verified)
{
    const struct dsa_ops *ops = NULL;

    for (size_t i = 0; i < CONFIG_CRYPTO_MAX_DSA_OPS; i++) {
        if (dsa_ops[i] && (dsa_ops[i]->alg_bits & alg)) {
            ops = dsa_ops[i];
            break;
        }
    }

    if (ops == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    return ops->verify(
        der_signature, signature_length, der_key, key_length, md_alg, md, md_length, verified);
}

int dsa_add_ops(const struct dsa_ops *ops)
{
    if (no_of_dsa_ops >= CONFIG_CRYPTO_MAX_DSA_OPS)
        return -PB_ERR_MEM;

    LOG_INFO("Register dsa ops: %s, 0x%x", ops->name, ops->alg_bits);
    dsa_ops[no_of_dsa_ops++] = ops;
    return PB_OK;
}

void hash_print(const char *prefix, uint8_t *digest, size_t length)
{
    printf("%s ", prefix);
    for (size_t i = 0; i < length; i++)
        printf("%02x", digest[i]);
    printf("\n\r");
}

#ifdef CONFIG_SELF_TEST
static const char abcdpq[] = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

static const uint8_t sha256_empty[] = "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14\x9a\xfb\xf4\xc8\x99\x6f\xb9"
                                      "\x24\x27\xae\x41\xe4\x64\x9b\x93\x4c\xa4\x95\x99\x1b\x78\x52"
                                      "\xb8\x55";
static const uint8_t sha256_abc[] = "\xba\x78\x16\xbf\x8f\x01\xcf\xea\x41\x41\x40\xde\x5d\xae\x22"
                                    "\x23\xb0\x03\x61\xa3\x96\x17\x7a\x9c\xb4\x10\xff\x61\xf2\x00"
                                    "\x15\xad";
static const uint8_t sha256_abcdpq[] = "\x24\x8d\x6a\x61\xd2\x06\x38\xb8\xe5\xc0\x26\x93\x0c\x3e"
                                       "\x60\x39\xa3\x3c\xe4\x59\x64\xff\x21\x67\xf6\xec\xed\xd4"
                                       "\x19\xdb\x06\xc1";
static const uint8_t sha256_1Ma[] = "\xcd\xc7\x6e\x5c\x99\x14\xfb\x92\x81\xa1\xc7\xe2\x84\xd7\x3e"
                                    "\x67\xf1\x80\x9a\x48\xa4\x97\x20\x0e\x04\x6d\x39\xcc\xc7\x11"
                                    "\x2c\xd0";

DECLARE_SELF_TEST(crypto_test_sha256)
{
    uint8_t hash_output[32];
    hash_init(HASH_SHA256);
    hash_final(hash_output, sizeof(hash_output));

    if (memcmp(hash_output, sha256_empty, 32) != 0) {
        LOG_ERR("Failed");
        hash_print("Expected", (uint8_t *)sha256_empty, 32);
        hash_print("Output", hash_output, 32);
        return -1;
    }

    return 0;
}

DECLARE_SELF_TEST(crypto_test_sha256_abc)
{
    uint8_t hash_output[32];
    hash_init(HASH_SHA256);
    hash_update("abc", 3);
    hash_final(hash_output, sizeof(hash_output));

    if (memcmp(hash_output, sha256_abc, 32) != 0) {
        LOG_ERR("Failed");
        hash_print("Expected", (uint8_t *)sha256_abc, 32);
        hash_print("Output", hash_output, 32);
        return -1;
    }

    return 0;
}

DECLARE_SELF_TEST(crypto_test_sha256_abcdpq)
{
    uint8_t hash_output[32];
    hash_init(HASH_SHA256);
    hash_update(abcdpq, strlen(abcdpq));
    hash_final(hash_output, sizeof(hash_output));

    if (memcmp(hash_output, sha256_abcdpq, 32) != 0) {
        LOG_ERR("Failed");
        hash_print("Expected", (uint8_t *)sha256_abcdpq, 32);
        hash_print("Output", hash_output, 32);
        return -1;
    }

    return 0;
}

static uint8_t hash_test_buf[1000];

DECLARE_SELF_TEST(crypto_test_sha256_1Ma)
{
    int rc;
    uint8_t hash_output[32];

    /* Test 1M 'a' */
    memset(hash_test_buf, 'a', 1000);
    hash_init(HASH_SHA256);

    for (int n = 0; n < 1000; n++) {
        rc = hash_update(hash_test_buf, 1000);
        if (rc != 0) {
            return rc;
        }
    }
    hash_final(hash_output, sizeof(hash_output));

    if (memcmp(hash_output, sha256_1Ma, 32) != 0) {
        LOG_ERR("Failed");
        hash_print("Expected", (uint8_t *)sha256_1Ma, 32);
        hash_print("Output", hash_output, 32);
        return -1;
    }

    return 0;
}

#endif
