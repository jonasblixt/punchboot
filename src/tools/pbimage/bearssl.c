#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pb/pb.h>
#include <pb/crypto.h>
#include <3pp/bearssl/bearssl_hash.h>
#include <3pp/bearssl/bearssl_rsa.h>
#include <3pp/bearssl/bearssl_ec.h>
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
    uint32_t hash_size = 0;
    struct stat finfo;

    switch (hash_kind)
    {   
        case PB_HASH_SHA256:
            hash_size = 32;
        break;
        case PB_HASH_SHA384:
            hash_size = 48;
        break;
        case PB_HASH_SHA512:
            hash_size = 64;
        break;
        default:
            return PB_ERR;
    }

    stat (key_source, &finfo);

    /* Create signature */
    br_skey_decoder_context skey_ctx;

    fp = fopen(key_source, "rb");

    if (fp == NULL)
    {
        printf("Could not open file\n");
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
    br_hash_class *hc;

    switch (sign_kind)
    {
        case PB_SIGN_EC256:
            hc = (br_hash_class *) &br_sha256_vtable;
        break;
        case PB_SIGN_EC384:
            hc = (br_hash_class *) &br_sha384_vtable;
        break;
        case PB_SIGN_EC521:
            hc = (br_hash_class *) &br_sha512_vtable;
        break;
        default:
        break;
    }

    switch (sign_kind)
    {
        case PB_SIGN_RSA4096:
        {
            printf("Creating RSA4096 signature\n");
            br_rsa_private_key *br_k;
            br_k = (br_rsa_private_key *) br_skey_decoder_get_rsa(&skey_ctx);

            if (br_k == NULL)
            {
                err = PB_ERR_IO;
                printf("Could not decode key");
                goto err_out3;
            }

            if (!br_rsa_i62_pkcs1_sign(NULL, hash, hash_size, br_k, out))
                err = PB_ERR;
        }
        break;
        case PB_SIGN_EC256:
        case PB_SIGN_EC384:
        case PB_SIGN_EC521:
        {
            br_ec_private_key *br_k;
            br_k = (br_ec_private_key *) br_skey_decoder_get_ec(&skey_ctx);

            if (br_k == NULL)
            {
                printf("Could not read key\n");
                err = PB_ERR_IO;
                goto err_out3;
            }

            if(!br_ecdsa_i31_sign_raw(&br_ec_prime_i31,hc,
                                hash,br_k,out))
                err = PB_ERR;
        }
        break;
        default:
            err = PB_ERR;
    }

err_out3:
    free (key_data);
err_out2:
    fclose(fp);
err_out1:
    return err;
}
