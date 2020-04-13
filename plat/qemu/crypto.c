/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/asn1.h>
#include <pb/crypto.h>
#include <pb/plat.h>
#include <bpak/bpak.h>
#include <bearssl/bearssl_hash.h>
#include <bearssl/bearssl_rsa.h>
#include <bearssl/bearssl_ec.h>

static br_sha256_context sha256_ctx;
static br_sha384_context sha384_ctx;
static br_sha512_context sha512_ctx;
static br_md5_context md5_ctx;
static uint8_t bearssl_private[4096];

int plat_hash_init(struct pb_hash_context *ctx,
                          enum pb_hash_algs alg)
{
    ctx->alg = alg;
    memset(ctx->buf, 0, sizeof(ctx->buf));
    LOG_DBG("%i", alg);
    switch (alg)
    {
        case PB_HASH_SHA256:
            br_sha256_init(&sha256_ctx);
        break;
        case PB_HASH_SHA384:
            br_sha384_init(&sha384_ctx);
        break;
        case PB_HASH_SHA512:
            br_sha512_init(&sha512_ctx);
        break;
        case PB_HASH_MD5:
            br_md5_init(&md5_ctx);
        break;
        default:
        case PB_HASH_INVALID:
            LOG_ERR("Invalid hash");
            return -PB_ERR;
    }

    return PB_OK;
}

int plat_hash_update(struct pb_hash_context *ctx,
                              void *buf, size_t size)
{

    if (!size)
        return PB_OK;

    LOG_DBG("%p, %zu", buf, size);
    switch (ctx->alg)
    {
        case PB_HASH_SHA256:
            br_sha256_update(&sha256_ctx, buf, size);
        break;
        case PB_HASH_SHA384:
            br_sha384_update(&sha384_ctx, buf, size);
        break;
        case PB_HASH_SHA512:
            br_sha512_update(&sha512_ctx, buf, size);
        break;
        case PB_HASH_MD5:
            br_md5_update(&md5_ctx, buf, size);
        break;
        default:
        case PB_HASH_INVALID:
            LOG_ERR("Invalid hash");
            return -PB_ERR;
    }
    return PB_OK;
}

int plat_hash_finalize(struct pb_hash_context *ctx,
                              void *buf, size_t size)
{

    LOG_DBG("%p, %zu", buf, size);

    switch (ctx->alg)
    {
        case PB_HASH_SHA256:
            if (size)
            {
                br_sha256_update(&sha256_ctx,
                                    buf, size);
            }

            br_sha256_out(&sha256_ctx, ctx->buf);
        break;
        case PB_HASH_SHA384:
            if (size)
            {
                br_sha384_update(&sha384_ctx,
                                    buf, size);
            }

            br_sha384_out(&sha384_ctx, ctx->buf);
        break;
        case PB_HASH_SHA512:
            if (size)
            {
                br_sha512_update(&sha512_ctx,
                                    buf, size);
            }
            br_sha512_out(&sha512_ctx, ctx->buf);
        break;
        case PB_HASH_MD5:
            if (size)
            {
                br_md5_update(&md5_ctx, buf, size);
            }

            br_md5_out(&md5_ctx, ctx->buf);
        break;
        default:
        case PB_HASH_INVALID:
            LOG_ERR("Invalid hash");
            return -PB_ERR;
    }
    return PB_OK;
}

int plat_pk_verify(void *signature, size_t size, struct pb_hash_context *hash,
                    struct bpak_key *key)
{
    int err;
    bool signature_verified = false;
    uint8_t *output_data = bearssl_private;

    switch (key->kind)
    {
        case BPAK_KEY_PUB_RSA4096:
        {
            LOG_DBG("Checking RSA4096 signature...");
            /*struct pb_rsa4096_key *rsa_key =
                (struct pb_rsa4096_key *) k->data;*/

            br_rsa_public_key br_k;
            br_k.nlen = 512;
            br_k.elen = 3;

            err = pb_asn1_rsa_data(key, &br_k.n, &br_k.e);

            if (err != PB_OK)
            {
                LOG_ERR("Could not decode RSA4096 key");
                return PB_ERR;
            }

            memcpy(output_data, signature, 512);

            if (!br_rsa_i62_public(output_data, 512, &br_k))
                return PB_ERR;

            uint32_t n = 0;
            uint32_t y = 0;
            for (uint32_t i = (512-32); i < 512; i++)
            {
                if (output_data[i] != hash->buf[n])
                    y++;
                n++;
            }

            if (y == 0)
            {
                signature_verified = true;
            }
            else
            {
                LOG_DBG("Signature error");
                return PB_ERR;
            }
        }
        break;
        case BPAK_KEY_PUB_PRIME256v1:
        {
            LOG_DBG("EC256");
            br_ec_public_key br_k;
            pb_asn1_eckey_data(key, &br_k.q, &br_k.qlen, true);
            br_k.curve = BR_EC_secp256r1;

            for (int i = 0; i < 32; i++)
                printf("%x", hash->buf[i]);
            printf("\n\r");
            err = br_ecdsa_i31_vrfy_asn1(&br_ec_all_m15, hash->buf, 32,
                                    &br_k, signature, size);
            signature_verified = (err == 1);

        }
        break;
        case BPAK_KEY_PUB_SECP384r1:
        {
            LOG_DBG("EC384");
            br_ec_public_key br_k;
            pb_asn1_eckey_data(key, &br_k.q, &br_k.qlen, true);
            br_k.curve = BR_EC_secp384r1;
            err = br_ecdsa_i31_vrfy_asn1(&br_ec_all_m15, hash->buf, 48,
                                    &br_k, signature, size);

            signature_verified = (err == 1);
        }
        case BPAK_KEY_PUB_SECP521r1:
        {
            LOG_DBG("EC521 %zu", size);
            br_ec_public_key br_k;
            pb_asn1_eckey_data(key, &br_k.q, &br_k.qlen, true);
            br_k.curve = BR_EC_secp521r1;
            err = br_ecdsa_i31_vrfy_asn1(&br_ec_all_m15, hash->buf, 64,
                                    &br_k, signature, size);

            signature_verified = (err == 1);
        }
        break;
        default:
            LOG_ERR("Unknown signature format");
            return PB_ERR;
        break;
    }

    if ( (signature_verified) )
        return PB_OK;

    return -PB_ERR;
}

int plat_crypto_init(void)
{
    return PB_OK;
}
