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
#include <utils_def.h>

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
    int (*copy_update)(uintptr_t src, uintptr_t dest, size_t length);
    int (*final)(uint8_t *digest_out, size_t length);
};

struct dsa_ops {
    const char *name;
    uint32_t alg_bits;
    int (*verify)(uint8_t *der_signature,
                  uint8_t *der_key,
                  uint8_t *md, size_t md_length,
                  bool *verified);
};

/* Hash interface */
int hash_init(hash_t alg);
int hash_update(uintptr_t buf, size_t length);
int hash_copy_update(uintptr_t src, uintptr_t dest, size_t length);
int hash_final(uint8_t *digest_output, size_t length);
int hash_add_ops(const struct hash_ops *ops);

/* DSA interface */
int dsa_verify(dsa_t alg,
               uint8_t *der_signature,
               uint8_t *der_key,
               uint8_t *md, size_t md_length,
               bool *verified);

int dsa_add_ops(const struct dsa_ops *ops);

void hash_print(const char *prefix, uint8_t *digest, size_t length);

#endif
