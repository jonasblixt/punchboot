/**
 * \file crypto.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PB_INCLUDE_CRYPTO_H
#define PB_INCLUDE_CRYPTO_H

#include <pb/utils_def.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * \def CRYPTO_MD_MAX_SZ
 * Largest message digest in bytes
 */
#define CRYPTO_MD_MAX_SZ 64

typedef uint32_t hash_t;
typedef uint32_t dsa_t;
typedef uint32_t key_id_t;

#define HASH_MD5         BIT(0)
#define HASH_MD5_BROKEN  BIT(1)
#define HASH_SHA256      BIT(2)
#define HASH_SHA384      BIT(3)
#define HASH_SHA512      BIT(4)

#define DSA_EC_SECP256r1 BIT(0)
#define DSA_EC_SECP384r1 BIT(1)
#define DSA_EC_SECP521r1 BIT(2)

struct hash_ops {
    const char *name; /*!< Name of hash op's provider */
    uint32_t alg_bits; /*!< Bit field that indicates supported algs */
    int (*init)(hash_t alg); /*!< Hash init call back */
    int (*update)(const void *buf, size_t length); /*!< Hash update callback */
    int (*update_async)(const void *buf, size_t length);
    /*!< Optional asynchronous update callback. The implementation is expected
     * to queue/prepare an hash update and block if it's called again, until
     * the current operation is completed */
    int (*copy_update)(const void *src, void *dest, size_t length);
    /*!< Optional copy and update. This function will simultaiously copy and
     * hash data */
    int (*final)(uint8_t *digest_out, size_t length);
    /*!< Finialize and output message digest */
};

struct dsa_ops {
    const char *name;
    uint32_t alg_bits;
    int (*verify)(const uint8_t *der_signature,
                  size_t signature_length,
                  const uint8_t *der_key,
                  size_t key_length,
                  hash_t md_alg,
                  uint8_t *md,
                  size_t md_length,
                  bool *verified);
};

/**
 * Initialize the hashing context. The crypto API only supports one running
 * context, calling this function will reset the context.
 *
 * param[in] alg Hashing algorithm to use
 *
 * @return PB_OK on success,
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
int hash_update(const void *buf, size_t length);

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
int hash_update_async(const void *buf, size_t length);

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
int hash_copy_update(const void *src, void *dest, size_t length);

/**
 * Finalize hashing context.
 *
 * This function will block if there is an async job queued.
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
               const uint8_t *der_signature,
               size_t signature_length,
               const uint8_t *der_key,
               size_t key_length,
               hash_t md_alg,
               uint8_t *md,
               size_t md_length,
               bool *verified);

int dsa_add_ops(const struct dsa_ops *ops);

void hash_print(const char *prefix, uint8_t *digest, size_t length);

#endif
