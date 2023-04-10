/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PB_INCLUDE_CRYPTO_H
#define PB_INCLUDE_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pb/utils_def.h>

#define CRYPTO_MD_MAX_SZ 64

typedef uint32_t hash_t;
typedef uint32_t dsa_t;

#define HASH_MD5        BIT(0)
#define HASH_MD5_BROKEN BIT(1)
#define HASH_SHA256     BIT(2)
#define HASH_SHA384     BIT(3)
#define HASH_SHA512     BIT(4)

#define DSA_EC_SECP256r1 BIT(0)
#define DSA_EC_SECP384r1 BIT(1)
#define DSA_EC_SECP521r1 BIT(2)

struct hash_ops {
    const char *name;
    uint32_t alg_bits;
    int (*init)(hash_t alg);
    int (*update)(uintptr_t buf, size_t length);
    int (*update_async)(uintptr_t buf, size_t length);
    int (*copy_update)(uintptr_t src, uintptr_t dest, size_t length);
    int (*final)(uint8_t *digest_out, size_t length);
};

struct dsa_ops {
    const char *name;
    uint32_t alg_bits;
    int (*verify)(uint8_t *der_signature, size_t signature_length,
                  uint8_t *der_key, size_t key_length,
                  hash_t md_alg, uint8_t *md, size_t md_length,
                  bool *verified);
};

/**
 * param[in] alg Hashing algorithm to use
 *
 * @return PB_OK on success
 *        -PB_ERR_PARAM, on invalid hash alg
 */
int hash_init(hash_t alg);

/**
 * Update currently running hash context with data
 *
 * @param[in] buf Input buffer to hash
 * @param[in] lenght Length of buffer
 *
 * @return PB_OK on sucess
 */
int hash_update(uintptr_t buf, size_t length);

/**
 * Update current running hash context with data.
 * This fuction might be implemented by drivers for hardware accelerated
 * hashing functions. Typically it will enqueue DMA descriptors and not wait
 * for completion.
 *
 * The underlying driver should check if there is a job in progress and block
 * before enqueueing additional descriptors.
 *
 * @param[in] buf Input buffer to hash
 * @param[in] length Length of buffer
 *
 * @return PB_OK on success
 */
int hash_update_async(uintptr_t buf, size_t length);

/**
 * Some hardware/drivers support both updating the hash context and
 * copy the input buffer to another memory destination.
 *
 * If the underlying driver does not implement the copy_update API the crypto
 * module will use memcpy.
 *
 * @param[in] src Input/Source buffer to hash/copy
 * @param[in] dest Destination address
 * @param[in] length Length of input buffer
 *
 * @return PB_OK on sucess
 */
int hash_copy_update(uintptr_t src, uintptr_t dest, size_t length);

/**
 * Finalize hashing context
 *
 * @param[out] digest_output Message digest output buffer
 * @param[in] lenght Length of output buffer
 *
 * @return PB_OK on success
 */
int hash_final(uint8_t *digest_output, size_t length);

/**
 * Register hash op's
 *
 * Used by drivers to expose hashing functions.
 *
 * @param[in] ops Hashing op's structure
 *
 * @return PB_OK on success
 */
int hash_add_ops(const struct hash_ops *ops);

/* DSA interface */
int dsa_verify(dsa_t alg,
               uint8_t *der_signature, size_t signature_length,
               uint8_t *der_key, size_t key_length,
               hash_t md_alg, uint8_t *md, size_t md_length,
               bool *verified);

int dsa_add_ops(const struct dsa_ops *ops);

void hash_print(const char *prefix, uint8_t *digest, size_t length);

#endif
