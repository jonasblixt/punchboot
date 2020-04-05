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
};

enum pb_key_types
{
    PB_KEY_INVALID,
    PB_KEY_RSA4096,
    PB_KEY_PRIME256v1,
    PB_KEY_SECP384r1,
    PB_KEY_SECP521r1,
};

struct pb_crypto_driver;

struct pb_hash_context
{
    uint8_t buf[128] __a16b;
    enum pb_hash_algs alg;
    struct pb_crypto_driver *driver;
    bool init;
};

typedef int (*pb_crypto_call_t) (struct pb_crypto_driver *drv);

typedef int (*pb_crypto_hash_init_t) (struct pb_crypto_driver *drv,
                                      struct pb_hash_context *ctx,
                                      enum pb_hash_algs alg);

typedef int (*pb_crypto_hash_io_t) (struct pb_crypto_driver *drv,
                                        struct pb_hash_context *ctx,
                                        void *buf, size_t size);

typedef int (*pb_crypto_pk_verify_t) (struct pb_crypto_driver *drv,
                                    struct pb_hash_context *hash,
                                    struct bpak_key *key,
                                    void *signature, size_t size);

struct pb_crypto_plat_driver
{
    pb_crypto_call_t init;
    pb_crypto_call_t free;
    void *private;
    size_t size;
};

struct pb_crypto_driver
{
    bool ready;
    pb_crypto_call_t init;
    pb_crypto_call_t free;
    pb_crypto_hash_init_t hash_init;
    pb_crypto_hash_io_t hash_update;
    pb_crypto_hash_io_t hash_final;
    pb_crypto_pk_verify_t pk_verify;
    struct pb_crypto_plat_driver *platform;
    void *private;
    size_t size;
    struct pb_crypto_driver *next;
};

struct pb_crypto
{
    struct pb_crypto_driver *drivers;
};

int pb_crypto_init(struct pb_crypto *crypto);
int pb_crypto_free(struct pb_crypto *crypto);
int pb_crypto_add(struct pb_crypto *crypto, struct pb_crypto_driver *drv);
int pb_crypto_start(struct pb_crypto *crypto);

int pb_hash_init(struct pb_crypto *crypto, struct pb_hash_context *ctx,
                        enum pb_hash_algs alg);
int pb_hash_update(struct pb_hash_context *ctx, void *buf, size_t size);
int pb_hash_finalize(struct pb_hash_context *ctx, void *buf, size_t size);

int pb_pk_verify(struct pb_crypto *crypto, void *signature, size_t size,
                    struct pb_hash_context *hash, struct bpak_key *key);

#endif  // INCLUDE_PB_CRYPTO_H_
