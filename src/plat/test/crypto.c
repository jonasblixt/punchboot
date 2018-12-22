#include <pb.h>
#include <plat.h>


uint32_t  plat_sha256_init(void)
{
    return PB_ERR;
}

uint32_t  plat_sha256_update(uint8_t *bfr, uint32_t sz)
{
    UNUSED(bfr);
    UNUSED(sz);
    return PB_ERR;
}

uint32_t  plat_sha256_finalize(uint8_t *out)
{
    UNUSED(out);
    return PB_ERR;
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


