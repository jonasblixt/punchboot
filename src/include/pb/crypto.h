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
#include <stddef.h>
#include <pb/pb.h>
#include <bpak/keystore.h>

enum pb_hash_algs
{
    PB_HASH_INVALID,
    PB_HASH_MD5,
    PB_HASH_SHA256,
    PB_HASH_SHA384,
    PB_HASH_SHA512,
    PB_HASH_MD5_BROKEN,
};

enum pb_key_types
{
    PB_KEY_INVALID,
    PB_KEY_RSA4096,
    PB_KEY_PRIME256v1,
    PB_KEY_SECP384r1,
    PB_KEY_SECP521r1,
};

struct pb_hash_context
{
    uint8_t buf[128] __a4k;
    enum pb_hash_algs alg;
    bool init;
};


#endif  // INCLUDE_PB_CRYPTO_H_
