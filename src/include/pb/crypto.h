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
#include <bpak/keystore.h>

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
    PB_SIGN_PRIME256v1,
    PB_SIGN_SECP384r1,
    PB_SIGN_SECP521r1,
};

enum
{
    PB_KEY_INVALID,
    PB_KEY_RSA4096,
    PB_KEY_PRIME256v1,
    PB_KEY_SECP384r1,
    PB_KEY_SECP521r1,
};

#define PB_HASH_BUF_SZ 128

struct pb_hash_context
{
    uint32_t hash_kind;
    uint8_t buf[PB_HASH_BUF_SZ];
};

#ifdef __PB_BUILD
int pb_asn1_eckey_data(struct bpak_key *k, uint8_t **data, uint8_t *key_sz);
int pb_asn1_ecsig_to_rs(uint8_t *sig, uint8_t sig_kind,
                            uint8_t **r, uint8_t **s);

int pb_asn1_rsa_data(struct bpak_key *k, uint8_t **mod, uint8_t **exp);
#endif

#endif  // INCLUDE_PB_CRYPTO_H_
