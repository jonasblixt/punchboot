#include <stdio.h>
#include <pb.h>
#include <plat.h>
#include <3pp/bearssl/bearssl_hash.h>
#include <3pp/bearssl/bearssl_rsa.h>

static br_sha256_context sha256_ctx;
static br_md5_context md5_ctx;
static uint32_t _hash_kind;

uint32_t  plat_hash_init(uint32_t hash_kind)
{
    _hash_kind = hash_kind;
    br_sha256_init(&sha256_ctx);
    br_md5_init(&md5_ctx);
    return PB_OK;
}

uint32_t  plat_hash_update(uintptr_t bfr, uint32_t sz)
{
    if (_hash_kind == PB_HASH_SHA256)
        br_sha256_update(&sha256_ctx, (void *) bfr,sz);
    else if (_hash_kind == PB_HASH_MD5)
        br_md5_update(&md5_ctx, (void *) bfr,sz);
    else
        return PB_ERR;

    return PB_OK;
}

uint32_t  plat_hash_finalize(uintptr_t out)
{
    if (_hash_kind == PB_HASH_SHA256)
        br_sha256_out(&sha256_ctx, (void *) out);
    else if (_hash_kind == PB_HASH_MD5)
        br_md5_out(&md5_ctx, (void *) out);
    else
        return PB_ERR;

    return PB_OK;
}

static __no_bss __a4k unsigned char output_data[1024];

uint32_t  plat_verify_signature(uint8_t *sig, uint32_t sig_kind,
                                uint8_t *hash, uint32_t hash_kind,
                                struct pb_key *k)
{
    UNUSED(hash_kind);
    bool signature_verified = false;

    switch (sig_kind)
    {
        case PB_SIGN_RSA4096:
        {
            LOG_DBG("Checking RSA4096 signature...");
            struct pb_rsa4096_key *rsa_key =
                (struct pb_rsa4096_key *) k->data;
            br_rsa_public_key br_k;
            
            br_k.n = (unsigned char*) rsa_key->mod;
            br_k.nlen = 512;
            br_k.e = (unsigned char*)rsa_key->exp;
            br_k.elen = 3;

            memcpy(output_data,sig,512);
            
            if (!br_rsa_i62_public(output_data, 512, &br_k))
                return PB_ERR;

            uint32_t n = 0;
            uint32_t y = 0;
            for (uint32_t i = (512-32); i < 512; i++) 
            {
                if (output_data[i] != hash[n]) 
                    y++;
                n++;
            }

            if (y == 0)
            {
                signature_verified = true;
            }
            else
            {
                LOG_DBG("Signature OK");
                return PB_ERR;
            }

        }
        break;
        default:
            LOG_ERR("Unknown signature format");
            return PB_ERR;
        break;
    }

    if ( (signature_verified) )
        return PB_OK;

    return PB_ERR;
}
