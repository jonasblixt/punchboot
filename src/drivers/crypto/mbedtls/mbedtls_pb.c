/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <stddef.h>
#include <stdlib.h>

#include <pb/crypto.h>
#include <drivers/crypto/mbedtls.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/md5.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/memory_buffer_alloc.h>
#include <mbedtls/platform.h>
#include <mbedtls/version.h>
#include <mbedtls/error.h>

static union {
    mbedtls_sha256_context sha256;
    mbedtls_sha512_context sha512;
    mbedtls_md5_context md5;
} mbed_hash;

static hash_t current_hash;

static int mbedtls_hash_init(hash_t pb_alg)
{
    current_hash = pb_alg;

    switch (pb_alg) {
    case HASH_SHA512:
        mbedtls_sha512_init(&mbed_hash.sha512);
        mbedtls_sha512_starts(&mbed_hash.sha512, 0);
        break;
    case HASH_SHA384:
        mbedtls_sha512_init(&mbed_hash.sha512);
        mbedtls_sha512_starts(&mbed_hash.sha512, 1);
        break;
    case HASH_SHA256:
        mbedtls_sha256_init(&mbed_hash.sha256);
        mbedtls_sha256_starts(&mbed_hash.sha256, 0);
        break;
    case HASH_MD5:
        mbedtls_md5_init(&mbed_hash.md5);
        mbedtls_md5_starts(&mbed_hash.md5);
    break;
    default:
        return -PB_ERR_PARAM;
    }


    return PB_OK;
}

static int mbedtls_hash_update(const void *buf, size_t length)
{
    switch (current_hash) {
    case HASH_SHA512:
        mbedtls_sha512_update(&mbed_hash.sha512, (const unsigned char *) buf, length);
        break;
    case HASH_SHA384:
        mbedtls_sha512_update(&mbed_hash.sha512, (const unsigned char *) buf, length);
        break;
    case HASH_SHA256:
        mbedtls_sha256_update(&mbed_hash.sha256, (const unsigned char *) buf, length);
        break;
    case HASH_MD5:
        mbedtls_md5_update(&mbed_hash.md5, (const unsigned char *) buf, length);
        break;
    default:
        return -PB_ERR_PARAM;
    }

    return PB_OK;
}

static int mbedtls_hash_final(uint8_t *output, size_t size)
{
    switch (current_hash) {
    case HASH_SHA512:
        if (size < 64)
            return -PB_ERR_BUF_TOO_SMALL;
        mbedtls_sha512_finish(&mbed_hash.sha512, (unsigned char *) output);
        break;
    case HASH_SHA384:
        if (size < 48)
            return -PB_ERR_BUF_TOO_SMALL;
        mbedtls_sha512_finish(&mbed_hash.sha512, (unsigned char *) output);
        break;
    case HASH_SHA256:
        if (size < 32)
            return -PB_ERR_BUF_TOO_SMALL;
        mbedtls_sha256_finish(&mbed_hash.sha256, (unsigned char *) output);
        break;
    case HASH_MD5:
        if (size < 16)
            return -PB_ERR_BUF_TOO_SMALL;
        mbedtls_md5_finish(&mbed_hash.md5, (unsigned char *) output);
        break;
    default:
        return -PB_ERR_PARAM;
    }

    return PB_OK;
}

static int mbed_ecda_verify(const uint8_t *der_signature, size_t signature_length,
                            const uint8_t *der_key, size_t key_length,
                            hash_t md_alg, uint8_t *md, size_t md_length,
                            bool *verified)
{
    int rc;
    char mbedtls_errorstr[256];
    mbedtls_pk_context ctx;
    mbedtls_md_type_t md_type;

    *verified = false;
    switch (md_alg) {
    case HASH_SHA512:
        md_type = MBEDTLS_MD_SHA512;
        break;
    case HASH_SHA384:
        md_type = MBEDTLS_MD_SHA384;
        break;
    case HASH_SHA256:
        md_type = MBEDTLS_MD_SHA256;
        break;
    default:
        return -PB_ERR_PARAM;
    }

    mbedtls_pk_init(&ctx);
    rc = mbedtls_pk_parse_public_key(&ctx, der_key, key_length);

    if (rc != 0) {
        mbedtls_strerror(rc, mbedtls_errorstr, sizeof(mbedtls_errorstr));
        LOG_ERR("Verify failed %s (%i)", mbedtls_errorstr, rc);
        return -PB_ERR_SIGNATURE;
    }

    rc = mbedtls_pk_verify(&ctx,
                           md_type, md, md_length,
                           der_signature, signature_length);

    mbedtls_pk_free(&ctx);

    if (rc == 0) {
        *verified = true;
        return PB_OK;
    } else {
        return -PB_ERR_SIGNATURE;
    }
}

int mbedtls_pb_init(void)
{
    int rc;
    static unsigned char heap[16*1024];
    mbedtls_memory_buffer_alloc_init(heap, sizeof(heap));

    static const struct hash_ops mbed_ops = {
        .name = "mbedtls-hash",
        .alg_bits = HASH_MD5 |
                    HASH_SHA256 |
                    HASH_SHA384 |
                    HASH_SHA512,
        .init = mbedtls_hash_init,
        .update = mbedtls_hash_update,
        .final = mbedtls_hash_final,
    };

    rc = hash_add_ops(&mbed_ops);

    if (rc != PB_OK)
        return rc;

    static const struct dsa_ops mbed_dsa_ops = {
        .name = "mbedtls-dsa",
        .alg_bits = DSA_EC_SECP256r1 |
                    DSA_EC_SECP384r1 |
                    DSA_EC_SECP521r1,
        .verify = mbed_ecda_verify,
    };

    rc = dsa_add_ops(&mbed_dsa_ops);

    if (rc != PB_OK)
        return rc;

    return PB_OK;
}
