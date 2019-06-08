/*===========================================================================*/
/**
    @file    srk_helper.c

    @brief   Provide helper functions to ease SRK tasks and also defines
             common struct that can used across different tools

@verbatim
=============================================================================

    Copyright 2018 NXP

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================
@endverbatim */

/*===========================================================================
                                INCLUDE FILES
=============================================================================*/
#include <string.h>
#include <strings.h>
#include "err.h"
#include "srk_helper.h"
#include "hab_types.h"
#include <openssl/rsa.h>

/*===========================================================================
                               GLOBAL FUNCTIONS
=============================================================================*/

/*--------------------------
  memcpy

  Basic memcpy implementation
  to decrease the LIBC dependencies to the version 2.7
---------------------------*/
void *
memcpy (void *dest, const void *src, size_t len)
{
    char *d = dest;
    const char *s = src;
    while (len--) {
        *d++ = *s++;
    }
    return dest;
}

/*--------------------------
  srk_entry_pkcs1
---------------------------*/
void
srk_entry_pkcs1(tgt_t target,
                EVP_PKEY *pkey,
                srk_entry_t *srk,
                bool ca_flag,
                const char *sd_alg_str)
{
    uint32_t idx = 0; /**< index */
    uint8_t *modulus; /**< Public key modulus */
    uint8_t *exponent; /**< Public key exponent */
    size_t mod_bytes; /**< Modulus size in bytes */
    size_t exp_bytes; /**< Exponent size in bytes */
    const BIGNUM *n, *e; /**< modulus and exponent defined as OpenSSL BIGNUM */

    RSA_get0_key(EVP_PKEY_get0_RSA(pkey), &n, &e, NULL);

    /* Get bigNUM modulus */
    modulus = get_bn(n, &mod_bytes);
    if (modulus == NULL)
    {
        error("Cannot allocate memory for modulus");
    }

    /* Get bigNUM exponent */
    exponent = get_bn(e, &exp_bytes);
    if (exponent == NULL)
    {
        error("Cannot allocate memory for exponent");
    }

    /* Build the entry */
    srk->entry = (uint8_t *)malloc(SRK_KEY_HEADER_BYTES +
                                   mod_bytes + exp_bytes);
    if (!srk->entry)
    {
        error("Cannot allocate SRK entry buffer");
    }

    srk->entry_bytes = SRK_KEY_HEADER_BYTES + mod_bytes + exp_bytes;

    /* Fill in header, ... */
    srk->entry[idx++] = HAB_KEY_PUBLIC;
    if(TGT_AHAB == target)
    {
        srk->entry[idx++] = EXTRACT_BYTE(srk->entry_bytes, 0);
        srk->entry[idx++] = EXTRACT_BYTE(srk->entry_bytes, 8);
        srk->entry[idx++] = HAB_ALG_PKCS1;

        /* Hash Algorithm */
        switch (digest_alg_tag(sd_alg_str))
        {
            case HAB_ALG_SHA256:
                srk->entry[idx++] = SRK_SHA256;
                break;

            case HAB_ALG_SHA384:
                srk->entry[idx++] = SRK_SHA384;
                break;

            case HAB_ALG_SHA512:
                srk->entry[idx++] = SRK_SHA512;
                break;

            default:
                error("Unsupported signature hash algorithm");
        }

        /* Key size */
        switch (EVP_PKEY_bits((EVP_PKEY *)pkey))
        {
            case 2048:
                srk->entry[idx++] = SRK_RSA2048;
                break;
            case 3072:
                srk->entry[idx++] = SRK_RSA3072;
                break;
            case 4096:
                srk->entry[idx++] = SRK_RSA4096;
                break;
            default:
                error("Unsupported RSA key size");
        }
    }
    else
    {
        srk->entry[idx++] = EXTRACT_BYTE(srk->entry_bytes, 8);
        srk->entry[idx++] = EXTRACT_BYTE(srk->entry_bytes, 0);
        srk->entry[idx++] = HAB_ALG_PKCS1;
        srk->entry[idx++] = 0;
        srk->entry[idx++] = 0;
    }
    srk->entry[idx++] = 0;

    /* Set CA flag in SRK consistent with the CA flag from the cert */
    if (ca_flag == TRUE)
    {
        srk->entry[idx++] = HAB_KEY_FLG_CA;
    }
    else
    {
        srk->entry[idx++] = 0;
    }

    /* then modulus bytes and exponent bytes, ... */
    if (TGT_AHAB != target)
    {
        srk->entry[idx++] = EXTRACT_BYTE(mod_bytes, 8);
        srk->entry[idx++] = EXTRACT_BYTE(mod_bytes, 0);
        srk->entry[idx++] = EXTRACT_BYTE(exp_bytes, 8);
        srk->entry[idx++] = EXTRACT_BYTE(exp_bytes, 0);
    }
    else
    {
        srk->entry[idx++] = EXTRACT_BYTE(mod_bytes, 0);
        srk->entry[idx++] = EXTRACT_BYTE(mod_bytes, 8);
        srk->entry[idx++] = EXTRACT_BYTE(exp_bytes, 0);
        srk->entry[idx++] = EXTRACT_BYTE(exp_bytes, 8);
    }

    /* followed by modulus, ...  */
    memcpy(&srk->entry[idx], modulus, mod_bytes);
    idx += mod_bytes;

    /* and finally exponent */
    memcpy(&srk->entry[idx], exponent, exp_bytes);

    idx += exp_bytes;
    srk->entry_bytes = idx;

    /* Clean up */
    free(exponent);
    free(modulus);
}

/*--------------------------
  srk_entry_ec
---------------------------*/
void
srk_entry_ec(tgt_t target,
             EVP_PKEY *pkey,
             srk_entry_t *srk,
             bool ca_flag,
             const char *sd_alg_str)
{
    uint32_t       idx = 0;       /**< Index                  */
    const EC_POINT *pub_key;      /**< EC public key          */
    BIGNUM         *num_x = NULL; /**< Public key number X    */
    BIGNUM         *num_y = NULL; /**< Public key number Y    */
    size_t         num_x_bytes;   /**< Number X size in bytes */
    size_t         num_y_bytes;   /**< Number Y size in bytes */
    size_t         num_bytes;
    uint8_t        hash_id = 0;
    uint8_t        curve_id = 0;

    /* NOTE - Function only used in AHAB context at the moment */

    pub_key = EC_KEY_get0_public_key(EVP_PKEY_get0_EC_KEY(pkey));
    if (NULL == pub_key)
    {
        error("Cannot retrive EC public key");
    }
    num_x = BN_new();
    if (NULL == num_x)
    {
        error("Cannot allocate memory for number X");
    }
    num_y = BN_new();
    if (NULL == num_y)
    {
        BN_free(num_x);
        error("Cannot allocate memory for number Y");
    }
    if (!EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(EVP_PKEY_get0_EC_KEY(pkey)),
                                             pub_key, num_x, num_y, NULL))
    {
        BN_free(num_x);
        BN_free(num_y);
        error("Cannot retrieve X and Y numbers");
    }

    num_x_bytes = BN_num_bytes(num_x);
    num_y_bytes = BN_num_bytes(num_y);

    switch (EVP_PKEY_bits((EVP_PKEY *)pkey))
    {
        case 256:
            num_bytes = 32;
            break;
        case 384:
            num_bytes = 48;
            break;
        case 521:
            num_bytes = 66;
            break;
        default:
            BN_free(num_x);
            BN_free(num_y);
            error("Unsupported ellyptic curve");
    }

    /* Build the entry */
    srk->entry = (uint8_t *)malloc(SRK_KEY_HEADER_BYTES +
                                   num_bytes * 2);
    if (NULL == srk->entry)
    {
        BN_free(num_x);
        BN_free(num_y);
        error("Cannot allocate SRK table entry buffer");
    }

    srk->entry_bytes = SRK_KEY_HEADER_BYTES + num_bytes * 2;

    if(TGT_AHAB == target)
    {
        /* Fill in header, ... */
        srk->entry[idx++] = HAB_KEY_PUBLIC;
        srk->entry[idx++] = EXTRACT_BYTE(srk->entry_bytes, 0);
        srk->entry[idx++] = EXTRACT_BYTE(srk->entry_bytes, 8);
        srk->entry[idx++] = SRK_ECDSA;
    }
    else
    {
        /* Fill in header, ... */
        srk->entry[idx++] = HAB_KEY_PUBLIC;
        srk->entry[idx++] = EXTRACT_BYTE(srk->entry_bytes, 8);
        srk->entry[idx++] = EXTRACT_BYTE(srk->entry_bytes, 0);
        srk->entry[idx++] = SRK_ECDSA;
    }


    /* Hash Algorithm */
    switch (digest_alg_tag(sd_alg_str))
    {
        case HAB_ALG_SHA256:
            hash_id = SRK_SHA256;
            break;

        case HAB_ALG_SHA384:
            hash_id = SRK_SHA384;
            break;

        case HAB_ALG_SHA512:
            hash_id = SRK_SHA512;
            break;

        default:
            BN_free(num_x);
            BN_free(num_y);
            error("Unsupported signature hash algorithm");
    }

    if(TGT_AHAB == target)
    {
        /* Curve */
        switch (EVP_PKEY_bits((EVP_PKEY *)pkey))
        {
        case 256:
            curve_id = SRK_PRIME256V1;
            break;
        case 384:
            curve_id = SRK_SEC384R1;
            break;
        case 521:
            curve_id = SRK_SEC521R1;
            break;
        default:
            BN_free(num_x);
            BN_free(num_y);
            error("Unsupported ellyptic curve");
        }
        srk->entry[idx++] = hash_id;
        srk->entry[idx++] = curve_id;
    }
    else
    {
        /* Curve */
        switch (EVP_PKEY_bits((EVP_PKEY *)pkey))
        {
        case 256:
            curve_id = HAB_EC_P256;
            break;
        case 384:
            curve_id = HAB_EC_P384;
            break;
        case 521:
            curve_id = HAB_EC_P521;
            break;
        default:
            BN_free(num_x);
            BN_free(num_y);
            error("Unsupported ellyptic curve");
        }
        srk->entry[idx++] = 0;
        srk->entry[idx++] = 0;
    }
    srk->entry[idx++] = 0;

    /* Set CA flag in SRK consistent with the CA flag from the cert */
    if (ca_flag == TRUE)
    {
        srk->entry[idx++] = HAB_KEY_FLG_CA;
    }
    else
    {
        srk->entry[idx++] = 0;
    }

    if(TGT_AHAB == target)
    {
        /* then number X bytes and number Y bytes, ... */
        srk->entry[idx++] = EXTRACT_BYTE(num_bytes, 0);
        srk->entry[idx++] = EXTRACT_BYTE(num_bytes, 8);
        srk->entry[idx++] = EXTRACT_BYTE(num_bytes, 0);
        srk->entry[idx++] = EXTRACT_BYTE(num_bytes, 8);
    }
    else
    {
        srk->entry[idx++] = curve_id;
        srk->entry[idx++] = 0;
        srk->entry[idx++] = EXTRACT_BYTE(EVP_PKEY_bits((EVP_PKEY *)pkey), 8);
        srk->entry[idx++] = EXTRACT_BYTE(EVP_PKEY_bits((EVP_PKEY *)pkey), 0);
    }

    /* followed by number X, ...  */
    if (num_bytes != num_x_bytes) {
        for (uint32_t i = 0; i < (num_bytes - num_x_bytes); i++) {
            srk->entry[idx++] = 0;
        }
    }
    BN_bn2bin(num_x, &srk->entry[idx]);
    idx += num_x_bytes;

    /* and finally number Y */
    if (num_bytes != num_y_bytes) {
        for (uint32_t i = 0; i < (num_bytes - num_y_bytes); i++) {
            srk->entry[idx++] = 0;
        }
    }
    BN_bn2bin(num_y, &srk->entry[idx]);
    idx += num_y_bytes;

    srk->entry_bytes = idx;

    /* Clean up */
    BN_free(num_x);
    BN_free(num_y);
}

/*--------------------------
  digest_alg_tag
---------------------------*/
uint32_t
digest_alg_tag(const char *digest_alg)
{
    uint32_t algorithm;

    if (digest_alg)
    {
        if (strncasecmp((digest_alg), HASH_ALG_SHA1,
                        sizeof(HASH_ALG_SHA1)) == 0)
        {
            algorithm = HAB_ALG_SHA1;
        }
        else if (strncasecmp((digest_alg), HASH_ALG_SHA256,
                             sizeof(HASH_ALG_SHA256)) == 0)
        {
            algorithm = HAB_ALG_SHA256;
        }
        else if (strncasecmp((digest_alg), HASH_ALG_SHA384,
                             sizeof(HASH_ALG_SHA384)) == 0)
        {
            algorithm = HAB_ALG_SHA384;
        }
        else if (strncasecmp((digest_alg), HASH_ALG_SHA512,
                             sizeof(HASH_ALG_SHA512)) == 0)
        {
            algorithm = HAB_ALG_SHA512;
        }
        else
        {
            error("Unsupported digest algorithm");
        }
    }
    else
    {
        error("Missing digest algorithm");
    }

    return algorithm;
}

/*--------------------------
  check_digest_alg
---------------------------*/
uint32_t
check_sign_digest_alg(const char *alg_str)
{
    uint32_t algorithm = digest_alg_tag(alg_str);

    if ((algorithm != HAB_ALG_SHA256)
        && (algorithm != HAB_ALG_SHA384)
        && (algorithm != HAB_ALG_SHA512))
    {
        error("Unsupported signature digest algorithm");
    }

    return algorithm;
}
