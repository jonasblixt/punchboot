#include <stdio.h>
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

uint32_t  plat_sha256_update(uintptr_t bfr, uint32_t sz)
{
    if (sha256_process(&md, (const unsigned char*) bfr, sz) != CRYPT_OK)
        return PB_ERR;
    return PB_OK;
}

uint32_t  plat_sha256_finalize(uintptr_t out)
{
    if (sha256_done(&md, (unsigned char *)out) != CRYPT_OK)
        return PB_ERR;
    return PB_OK;
}


unsigned char tmpbuf[1024];

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
          struct asn1_key *k)
{
    uint32_t x;
    uint32_t err = PB_OK;

    int e = 0;
    rsa_key key;

    e = rsa_set_key((const unsigned char *) k->mod, 512,
                    (const unsigned char *) k->exp, 3,
                    NULL, 0,
                    &key);

    if (e != CRYPT_OK)
    {
        LOG_ERR("rsa_set_key failed (%u)",e);
        return PB_ERR;
    }

    x = 1024;
    e = ltc_mp.rsa_me(sig, sig_sz, tmpbuf, &x, PK_PUBLIC, &key);
    
    if (e != CRYPT_OK)
    {
        LOG_ERR("RSA operation failed (%u)", e);
        return PB_ERR;
    }
    
    if (sig_sz != x)
    {
        LOG_ERR("sig_sz != x, x = %lu, sig_sz = %lu",x,sig_sz);
        return PB_ERR;
    }

    memcpy(out, tmpbuf, 512);
    return err;
}


