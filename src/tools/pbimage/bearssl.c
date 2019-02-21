#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pb/pb.h>
#include <3pp/bearssl/bearssl_hash.h>
#include <3pp/bearssl/bearssl_rsa.h>
#include <3pp/bearssl/bearssl_x509.h>

#include "crypto.h"

uint32_t crypto_initialize(void)
{
    return PB_OK;
}

uint32_t crypto_sign(uint8_t *hash,
                     uint32_t hash_kind,
                     const char *key_source,
                     uint32_t sign_kind,
                     uint8_t *out)
{
    FILE *fp = NULL;
    void *key_data = NULL;
    uint32_t err = PB_OK;
    struct stat finfo;

    if (hash_kind != PB_HASH_SHA256)
        return PB_ERR;
    if (sign_kind != PB_SIGN_RSA4096)
        return PB_ERR;

    stat (key_source, &finfo);

    /* Create signature */
    br_rsa_private_key *br_k;
    br_skey_decoder_context skey_ctx;

    fp = fopen(key_source, "rb");

    if (fp == NULL)
    {
        err = PB_ERR_IO;
        goto err_out1;
    }

    key_data = malloc(finfo.st_size);

    if (key_data == NULL)
    {
        err = PB_ERR_MEM;
        goto err_out2;
    }

    /* Load private key for signing */
    int key_sz = fread (key_data, 1, finfo.st_size, fp);

    br_skey_decoder_init(&skey_ctx);
    br_skey_decoder_push(&skey_ctx, key_data, key_sz);

    br_k = (br_rsa_private_key *) br_skey_decoder_get_rsa(&skey_ctx);

    if (br_k == NULL)
    {
        err = PB_ERR_IO;
        goto err_out3;
    }

    if (!br_rsa_i62_pkcs1_sign(NULL, hash, 32, br_k, out))
        err = PB_ERR;

err_out3:
    free (key_data);
err_out2:
    fclose(fp);
err_out1:
    return err;
}
