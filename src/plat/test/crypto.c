#include <stdio.h>
#include <pb.h>
#include <plat.h>

uint32_t  plat_sha256_init(void)
{
    return PB_OK;
}

uint32_t  plat_sha256_update(uintptr_t bfr, uint32_t sz)
{
    return PB_OK;
}

uint32_t  plat_sha256_finalize(uintptr_t out)
{
    return PB_OK;
}

uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
          struct asn1_key *k)
{
    return PB_ERR;
}


