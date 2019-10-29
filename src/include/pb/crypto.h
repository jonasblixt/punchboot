/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_CRYPTO_H_
#define INCLUDE_PB_CRYPTO_H_

#include <stdint.h>
#include <stdbool.h>

enum
{
    PB_HASH_INVALID,
    PB_HASH_MD5,
    PB_HASH_SHA256,
    PB_HASH_SHA384,
    PB_HASH_SHA512,
};

enum
{
    PB_SIGN_INVALID,
    PB_SIGN_RSA4096,
    PB_SIGN_NIST256p,
    PB_SIGN_NIST384p,
    PB_SIGN_NIST521p,
};

enum
{
    PB_KEY_INVALID,
    PB_KEY_RSA4096,
    PB_KEY_NIST256p,
    PB_KEY_NIST384p,
    PB_KEY_NIST521p,
};

#define PB_HASH_BUF_SZ 128

struct pb_hash_context
{
    uint32_t hash_kind;
    uint8_t buf[PB_HASH_BUF_SZ];
};

struct pb_rsa4096_key
{
    const uint8_t mod[512];
    const uint8_t exp[3];
};

struct pb_ec_key
{
    const __attribute__((aligned(4))) uint8_t public_key[132];
};

struct pb_key
{
    const uint32_t kind;
    const uint32_t id;
    const void *data;
};

struct pb_crypto_backend
{
};

#ifdef __PB_BUILD
uint32_t pb_crypto_init(struct pb_crypto_backend *backend);
uint32_t pb_crypto_get_key(uint32_t key_index, struct pb_key **k);
#endif

#endif  // INCLUDE_PB_CRYPTO_H_
