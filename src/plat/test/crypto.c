#include <stdio.h>
#include <pb.h>
#include <plat.h>
#include <3pp/bearssl/bearssl_hash.h>
#include <3pp/bearssl/bearssl_rsa.h>

static br_sha256_context sha256_ctx;

uint32_t  plat_sha256_init(void)
{
    br_sha256_init(&sha256_ctx);
    return PB_OK;
}

uint32_t  plat_sha256_update(uintptr_t bfr, uint32_t sz)
{
    br_sha256_update(&sha256_ctx, (void *) bfr,sz);
    return PB_OK;
}

uint32_t  plat_sha256_finalize(uintptr_t out)
{
    br_sha256_out(&sha256_ctx, (void *) out);
    return PB_OK;
}

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
          struct asn1_key *k)
{
    br_rsa_public_key br_k;
    
    br_k.n = k->mod;
    br_k.nlen = 512;
    br_k.e = k->exp;
    br_k.elen = 3;

    memcpy(out,sig,512);
    
    if (br_rsa_i62_public(out, sig_sz, &br_k))
        return PB_OK;

    return PB_ERR;
}


