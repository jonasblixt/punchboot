/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string.h>
#include <inttypes.h>
#include <pb/pb.h>
#include <pb/crypto.h>
#include <pb/self_test.h>

static const struct dsa_ops *dsa_ops[CONFIG_CRYPTO_MAX_DSA_OPS];
static size_t no_of_dsa_ops;
static const struct hash_ops *hash_ops[CONFIG_CRYPTO_MAX_HASH_OPS];
static size_t no_of_hash_ops;
static const struct hash_ops *current_hash_ops;  /* Currently active context */

int hash_init(hash_t alg)
{
    current_hash_ops = NULL;

    for (int i = 0; i < CONFIG_CRYPTO_MAX_HASH_OPS; i++) {
        if (hash_ops[i]->alg_bits & alg) {
            current_hash_ops = hash_ops[i];
            return current_hash_ops->init(alg);
        }
    }

    return PB_ERR_NOT_SUPPORTED;
}

int hash_update(uintptr_t buf, size_t length)
{
    if (current_hash_ops == NULL)
        return -PB_ERR_STATE;

    return current_hash_ops->update(buf, length);
}

int hash_copy_update(uintptr_t src, uintptr_t dest, size_t length)
{
    if (current_hash_ops == NULL)
        return -PB_ERR_STATE;

    if (current_hash_ops->copy_update != NULL) {
        return current_hash_ops->copy_update(src, dest, length);
    } else {
        memcpy((void*) dest, (void*) src, length);
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
               uint8_t *der_signature,
               uint8_t *der_key,
               uint8_t *md, size_t md_length,
               bool *verified)
{
    const struct dsa_ops *ops = NULL;

    for (size_t i = 0; i < CONFIG_CRYPTO_MAX_DSA_OPS; i++) {
        if (dsa_ops[i]->alg_bits & alg) {
            ops = dsa_ops[i];
            break;
        }
    }

    if (ops == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    return ops->verify(der_signature,
                        der_key,
                        md, md_length,
                        verified);
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
    printf("%s", prefix);
    for (size_t i = 0; i < length; i++)
        printf("%02x", digest[i]);
    printf("\n\r");
}
