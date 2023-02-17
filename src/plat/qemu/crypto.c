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
#include <pb/plat.h>
#include <bpak/bpak.h>
#include <bearssl/bearssl_hash.h>
#include <bearssl/bearssl_rsa.h>
#include <bearssl/bearssl_ec.h>

struct globals {
    union {
        br_sha256_context sha256_ctx;
        br_sha384_context sha384_ctx;
        br_sha512_context sha512_ctx;
        br_md5_context md5_ctx;
    };
    enum pb_hash_algs alg;
};

static struct globals globals;

int plat_hash_init(enum pb_hash_algs alg)
{
    globals.alg = alg;

    switch (alg)
    {
        case PB_HASH_SHA256:
            br_sha256_init(&globals.sha256_ctx);
        break;
        case PB_HASH_SHA384:
            br_sha384_init(&globals.sha384_ctx);
        break;
        case PB_HASH_SHA512:
            br_sha512_init(&globals.sha512_ctx);
        break;
        case PB_HASH_MD5:
            br_md5_init(&globals.md5_ctx);
        break;
        default:
        case PB_HASH_INVALID:
            LOG_ERR("Invalid hash");
            return -PB_ERR;
    }

    return PB_OK;
}

int plat_hash_update(uint8_t *buf, size_t len)
{
    LOG_DBG("%p, %zu", buf, len);

    switch (globals.alg) {
        case PB_HASH_SHA256:
            br_sha256_update(&globals.sha256_ctx, buf, len);
        break;
        case PB_HASH_SHA384:
            br_sha384_update(&globals.sha384_ctx, buf, len);
        break;
        case PB_HASH_SHA512:
            br_sha512_update(&globals.sha512_ctx, buf, len);
        break;
        case PB_HASH_MD5:
            br_md5_update(&globals.md5_ctx, buf, len);
        break;
        default:
        case PB_HASH_INVALID:
            LOG_ERR("Invalid hash");
            return -PB_ERR;
    }
    return PB_OK;
}

int plat_hash_output(uint8_t *output, size_t len)
{
    LOG_DBG("%p, %zu", output, len);

    switch (globals.alg) {
        case PB_HASH_SHA256:
            if (len < PB_HASH_SHA256_LEN)
                return -PB_ERR_BUF_TOO_SMALL;
            br_sha256_out(&globals.sha256_ctx, output);
        break;
        case PB_HASH_SHA384:
            if (len < PB_HASH_SHA384_LEN)
                return -PB_ERR_BUF_TOO_SMALL;
            br_sha384_out(&globals.sha384_ctx, output);
        break;
        case PB_HASH_SHA512:
            if (len < PB_HASH_SHA512_LEN)
                return -PB_ERR_BUF_TOO_SMALL;
            br_sha512_out(&globals.sha512_ctx, output);
        break;
        case PB_HASH_MD5:
            if (len < PB_HASH_MD5_LEN)
                return -PB_ERR_BUF_TOO_SMALL;
            br_md5_out(&globals.md5_ctx, output);
        break;
        default:
        case PB_HASH_INVALID:
            LOG_ERR("Invalid hash");
            return -PB_ERR;
    }
    return PB_OK;
}

int plat_pk_verify(uint8_t *signature, size_t signature_len,
                   uint8_t *hash, enum pb_hash_algs hash_kind,
                   struct bpak_key *key)
{
    int err;
    bool signature_verified = false;

    switch (key->kind) {
        case BPAK_KEY_PUB_PRIME256v1:
        {
            LOG_DBG("EC256");
            br_ec_public_key br_k;
            pb_asn1_eckey_data(key, &br_k.q, &br_k.qlen, true);
            br_k.curve = BR_EC_secp256r1;

            for (int i = 0; i < 32; i++)
                printf("%x", hash[i]);
            printf("\n\r");
            err = br_ecdsa_i31_vrfy_asn1(&br_ec_prime_i31, hash, 32,
                                    &br_k, signature, signature_len);
            signature_verified = (err == 1);

        }
        break;
        case BPAK_KEY_PUB_SECP384r1:
        {
            LOG_DBG("EC384");
            br_ec_public_key br_k;
            pb_asn1_eckey_data(key, &br_k.q, &br_k.qlen, true);
            br_k.curve = BR_EC_secp384r1;
            err = br_ecdsa_i31_vrfy_asn1(&br_ec_prime_i31, hash, 48,
                                    &br_k, signature, signature_len);

            signature_verified = (err == 1);
        }
        break;
        case BPAK_KEY_PUB_SECP521r1:
        {
            LOG_DBG("EC521");
            br_ec_public_key br_k;
            pb_asn1_eckey_data(key, &br_k.q, &br_k.qlen, true);
            br_k.curve = BR_EC_secp521r1;
            err = br_ecdsa_i31_vrfy_asn1(&br_ec_prime_i31, hash, 64,
                                    &br_k, signature, signature_len);

            signature_verified = (err == 1);
        }
        break;
        default:
            LOG_ERR("Unknown signature format");
            return PB_ERR;
        break;
    }

    if ( (signature_verified) )
    {
        LOG_INFO("Sig OK");
        return PB_OK;
    }

    LOG_ERR("Sig error");
    return -PB_ERR;
}

int plat_crypto_init(void)
{
    return PB_OK;
}
