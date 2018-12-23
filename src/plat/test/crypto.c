#include <pb.h>
#include <plat.h>
#include <tomcrypt.h>

static hash_state md;

uint32_t  plat_sha256_init(void)
{
    extern const ltc_math_descriptor ltm_desc;
    ltc_mp = ltm_desc;

    if (sha256_init(&md) != CRYPT_OK)
        return PB_ERR;
    return PB_OK;
}

uint32_t  plat_sha256_update(uint8_t *bfr, uint32_t sz)
{
    if (sha256_process(&md, (const unsigned char*) bfr, sz) != CRYPT_OK)
        return PB_ERR;
    return PB_OK;
}

uint32_t  plat_sha256_finalize(uint8_t *out)
{
    if (sha256_done(&md, out) != CRYPT_OK)
        return PB_ERR;
    return PB_OK;
}

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
          struct asn1_key *k)
{
    UNUSED(sig);
    UNUSED(sig_sz);
    UNUSED(out);
    UNUSED(k);
    return PB_ERR;
}


