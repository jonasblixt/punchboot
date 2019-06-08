/*===========================================================================*/
/**
    @file    openssl_helper.c

    @brief   Provide helper functions to ease openssl tasks. Mainly to
                provide common code for several tools.

@verbatim
=============================================================================

        (c) Freescale Semiconductor, Inc. 2011, 2012. All rights reserved.
        Copyright 2018-2019 NXP

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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "openssl_helper.h"
#include "version.h"
#include <openssl/rand.h>
#include <openssl/rsa.h>

/*===========================================================================
                               LOCAL CONSTANTS
=============================================================================*/

/*===========================================================================
                                 LOCAL MACROS
=============================================================================*/

/*===========================================================================
                  LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/

/*===========================================================================
                               OPENSSL 1.0.2 SUPPORT
=============================================================================*/

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)

static void *
OPENSSL_zalloc(size_t num)
{
    void *ret = OPENSSL_malloc(num);

    if (ret != NULL) {
        memset(ret, 0, num);
    }
    return ret;
}

void
ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps)
{
    if (pr != NULL) {
        *pr = sig->r;
    }
    if (ps != NULL) {
        *ps = sig->s;
    }
}

int
ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s)
{
    if (r == NULL || s == NULL) {
        return 0;
    }
    BN_clear_free(sig->r);
    BN_clear_free(sig->s);
    sig->r = r;
    sig->s = s;
    return 1;
}

void
EVP_MD_CTX_free(EVP_MD_CTX *ctx)
{
    EVP_MD_CTX_cleanup(ctx);
    OPENSSL_free(ctx);
}

EVP_MD_CTX *
EVP_MD_CTX_new(void)
{
    return OPENSSL_zalloc(sizeof(EVP_MD_CTX));
}

EC_KEY *
EVP_PKEY_get0_EC_KEY(EVP_PKEY *pkey)
{
    return (pkey->pkey.ec);
}

RSA *
EVP_PKEY_get0_RSA(EVP_PKEY *pkey)
{
    if (pkey->type != EVP_PKEY_RSA) {
        return NULL;
    }
    return pkey->pkey.rsa;
}

void
RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
{
    if (n != NULL) {
        *n = r->n;
    }
    if (e != NULL) {
        *e = r->e;
    }
    if (d != NULL) {
       *d = r->d;
    }
}

#endif

/*===========================================================================
                          LOCAL FUNCTION PROTOTYPES
=============================================================================*/

/*===========================================================================
                               LOCAL FUNCTIONS
=============================================================================*/

/*===========================================================================
                               GLOBAL FUNCTIONS
=============================================================================*/

/*--------------------------
  openssl_initialize
---------------------------*/

void
openssl_initialize(void)
{
#if defined _WIN32 || defined __CYGWIN__
    /* Required to avoid OpenSSL runtime errors on Win32 platforms */
    /* See: https://www.openssl.org/docs/faq.html#PROG3 */
    OPENSSL_malloc_init();
#endif

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();
#endif
}


/*--------------------------
  generate_hash
---------------------------*/

uint8_t *
generate_hash(const uint8_t *buf, size_t msg_bytes, const char *hash_alg,
              size_t *hash_bytes)
{
    const EVP_MD *type;                /**< Mesage digest type*/
    EVP_MD_CTX   *ctx = EVP_MD_CTX_new(); /**< Message digest context */
    uint8_t      *hash_mem_ptr = NULL; /**< location of result buffer */
    unsigned int  tmp;

    if (!(type = EVP_get_digestbyname(hash_alg)))
    {
        return NULL;
    }

    if (!(hash_mem_ptr = (uint8_t *)malloc(EVP_MAX_MD_SIZE)))
    {
        return NULL;
    }


    EVP_DigestInit(ctx, type);
    EVP_DigestUpdate(ctx, buf, msg_bytes);
    EVP_DigestFinal(ctx, hash_mem_ptr, &tmp);

    *hash_bytes = tmp;

    return hash_mem_ptr;
}

/*--------------------------
  get_bn
---------------------------*/

uint8_t*
get_bn(const BIGNUM *a, size_t *bytes)
{
    uint8_t *byte_array = NULL; /**< Resulting big number byte array */
    uint32_t a_num_bytes = BN_num_bytes(a);

    byte_array = malloc(a_num_bytes);
    if (byte_array == NULL)
    {
        return NULL;
    }

    BN_bn2bin(a, byte_array);

    *bytes = a_num_bytes;
    return byte_array;
}

/*--------------------------
  sign_data
---------------------------*/

uint8_t*
sign_data(const EVP_PKEY *skey, const BUF_MEM *bptr, hash_alg_t hash_alg,
          size_t *sig_bytes)
{
    EVP_MD_CTX   *ctx = EVP_MD_CTX_create(); /**< Signature context */
    uint8_t      *sig_buf = NULL;            /**< Location of sig. array */
    const EVP_MD *hash_type = NULL;          /**< Hash digest algorithm type */
    unsigned int  tmp_sig_bytes;
#ifdef DEBUG
    uint32_t i;                              /**< Loop index */
#endif

    if (ctx)
    {
        tmp_sig_bytes = EVP_PKEY_size((EVP_PKEY *)skey);
        sig_buf = malloc(tmp_sig_bytes);

        /* Determine OpenSSL hash digest type */
        if (hash_alg == SHA_1)
        {
            hash_type = EVP_sha1();
        }
        else if (hash_alg == SHA_256)
        {
            hash_type = EVP_sha256();
        }
        else
        {
            EVP_MD_CTX_destroy(ctx);
            return NULL;
        }

        if ((sig_buf != NULL) &&
            ( (EVP_SignInit_ex(ctx, hash_type, NULL) != CST_SUCCESS) ||
              (EVP_SignUpdate(ctx, bptr->data, bptr->length) != CST_SUCCESS) ||
              (EVP_SignFinal(ctx, sig_buf, &tmp_sig_bytes, (EVP_PKEY *)skey)
               != CST_SUCCESS) ))
        {
            *sig_bytes = tmp_sig_bytes;
            EVP_MD_CTX_destroy(ctx);
            return NULL;
        }

        *sig_bytes = tmp_sig_bytes;

#ifdef DEBUG
        printf("Signature bytes = %d\n", tmp_sig_bytes);
        if (sig_buf)
        {
            for (i = 0; i < tmp_sig_bytes; i++)
            {
                printf("%d) 0x%02x\n", i, sig_buf[i]);
            }
        }
#endif

        EVP_MD_CTX_destroy(ctx);
    }
    return sig_buf;
}

/*--------------------------
  read_certificate
---------------------------*/
X509*
read_certificate(const char* filename)
{
    BIO  *bio_cert = NULL; /**< OpenSSL BIO ptr */
    X509 *cert = NULL;     /**< X.509 certificate data structure */
    FILE *fp = NULL;       /**< File pointer for DER encoded file */
    /** Points to expected location of ".pem" filename extension */
    const char *temp = filename + strlen(filename) -
                       PEM_FILE_EXTENSION_BYTES;

    bio_cert = BIO_new(BIO_s_file());
    if (bio_cert == NULL)
    {
        return NULL;
    }

    /* PEM encoded */
    if (!strncasecmp(temp, PEM_FILE_EXTENSION, PEM_FILE_EXTENSION_BYTES))
    {
        if (BIO_read_filename(bio_cert, filename) <= 0)
        {
            BIO_free(bio_cert);
            return NULL;
        }

        cert = PEM_read_bio_X509(bio_cert, NULL, 0, NULL);
    }
    /* DER encoded */
    else
    {
        /* Open the DER file and load it into a X509 object */
        fp = fopen(filename, "rb");
        if (NULL == fp) return NULL;
        cert = d2i_X509_fp(fp, NULL);
        fclose(fp);
    }

    BIO_free(bio_cert);
    return cert;
}

/*--------------------------------
  get_der_encoded_certificate_data
----------------------------------*/
int32_t get_der_encoded_certificate_data(const char* filename,
                                         uint8_t ** der)
{
    /** Used for returning either size of der data or 0 to indicate an error */
    int32_t ret_val = 0;

    /* Read X509 certificate data from cert file */
    X509 *cert = read_certificate(filename);

    if (cert != NULL)
    {
        /* i2d_X509() allocates memory for der data, converts the X509
         * cert structure to binary der formatted data.  It then
         * returns the address of the memory allocated for the der data
         */
        ret_val = i2d_X509(cert, der);

        /* On error return 0 */
        if (ret_val < 0)
        {
            ret_val = 0;
        }
        X509_free(cert);
    }
    return ret_val;
}

/*--------------------------
  read_private_key
---------------------------*/
EVP_PKEY*
read_private_key(const char *filename, pem_password_cb *password_cb,
                 const char *password)
{
    BIO      *private_key = NULL; /**< OpenSSL BIO ptr */
    EVP_PKEY *pkey;               /**< Private Key data structure */
    /** Points to expected location of ".pem" filename extension */
    const char *temp = filename + strlen(filename) -
                       PEM_FILE_EXTENSION_BYTES;

    /* Read Private key */
    private_key = BIO_new(BIO_s_file( ));
    if (!private_key)
    {
        return NULL;
    }

    /* Set BIO to read from the given filename */
    if (BIO_read_filename(private_key, filename) <= 0)
    {
        BIO_free(private_key);
        return NULL;
    }

    if (!strncasecmp(temp, PEM_FILE_EXTENSION, PEM_FILE_EXTENSION_BYTES))
    {
        /* Read Private key - from PEM encoded file */
        pkey = PEM_read_bio_PrivateKey(private_key, NULL, password_cb,
                                       (char *)password);
        if (!pkey)
        {
            BIO_free(private_key);
            return NULL;
        }
    }
    else
    {
        pkey = d2i_PKCS8PrivateKey_bio (private_key, NULL, password_cb,
                                        (char *)password );
        if (!pkey)
        {
            BIO_free(private_key);
            return NULL;
        }
    }
    return pkey;
}

/*--------------------------
  print_version
---------------------------*/

void print_version(void)
{
    printf("Code Signing Tool release version %s\n",CST_VERSION);
}

/*--------------------------
  seed_prng
---------------------------*/
uint32_t seed_prng(uint32_t bytes)
{
    return RAND_load_file("/dev/random", bytes);
}


/*--------------------------
  gen_random_bytes
---------------------------*/
int32_t gen_random_bytes(uint8_t *buf, size_t bytes)
{
    if (!RAND_bytes(buf, bytes))
    {
        return CAL_RAND_API_ERROR;
    }

    return CAL_SUCCESS;
}

/*--------------------------
  get_digest_name
---------------------------*/
char*
get_digest_name(hash_alg_t hash_alg)
{
    char *hash_name = NULL;    /**< Ptr to return address of string macro */
    switch(hash_alg) {
        case SHA_1:
            hash_name = HASH_ALG_SHA1;
            break;
        case SHA_256:
            hash_name = HASH_ALG_SHA256;
            break;
        case SHA_384:
            hash_name = HASH_ALG_SHA384;
            break;
        case SHA_512:
            hash_name = HASH_ALG_SHA512;
            break;
        default:
            hash_name = HASH_ALG_INVALID;
            break;
    }
    return hash_name;
}

/*--------------------------
  calculate_hash
---------------------------*/
int32_t
calculate_hash(const char *in_file,
               hash_alg_t hash_alg,
               uint8_t *buf,
               int32_t *pbuf_bytes)
{
    const EVP_MD *sign_md; /**< Ptr to digest name */
    int32_t bio_bytes; /**< Length of bio data */
    BIO *in = NULL; /**< Ptr to BIO for reading data from in_file */
    BIO *bmd = NULL; /**< Ptr to BIO with hash bytes */
    BIO *inp; /**< Ptr to BIO for appending in with bmd */
    /** Status initialized to API error */
    int32_t err_value =  CAL_CRYPTO_API_ERROR;

    sign_md = EVP_get_digestbyname(get_digest_name(hash_alg));
    if (sign_md == NULL) {
        return CAL_INVALID_ARGUMENT;
    }

    /* Read data to generate hash */
    do {

        /* Create necessary bios */
        in = BIO_new(BIO_s_file());
        bmd = BIO_new(BIO_f_md());
        if (in == NULL || bmd == NULL) {
            break;
        }

        /* Set BIO to read filename in_file */
        if (BIO_read_filename(in, in_file) <= 0) {
            break;
        }

        /* Set BIO md to given hash */
        if (!BIO_set_md(bmd, sign_md)) {
            break;
        }

        /* Appends BIO in to bmd */
        inp = BIO_push(bmd, in);

        /* Read data from file BIO */
        do
        {
            bio_bytes = BIO_read(inp, (uint8_t *)buf, *pbuf_bytes);
        } while (bio_bytes > 0);

        /* Check for read error */
        if (bio_bytes < 0) {
            break;
        }

        /* Get the hash */
        bio_bytes = BIO_gets(inp, (char *)buf, *pbuf_bytes);
        if (bio_bytes <= 0) {
            break;
        }

        /* Send the output bytes in pbuf_bytes */
        *pbuf_bytes = bio_bytes;
        err_value =  CAL_SUCCESS;
    } while(0);

    if (in != NULL) BIO_free(in);
    if (bmd != NULL) BIO_free(bmd);

    return err_value;
}

/*--------------------------
  ver_sig_data
---------------------------*/
int32_t ver_sig_data(const char *in_file,
                     const char *cert_file,
                     hash_alg_t hash_alg,
                     sig_fmt_t  sig_fmt,
                     uint8_t    *sig_buf,
                     size_t     sig_buf_bytes)
{
    EVP_PKEY *pkey                = X509_get_pubkey(read_certificate(cert_file));
    const EVP_MD   *hash_type     = EVP_get_digestbyname(get_digest_name(hash_alg));
    int32_t        hash_bytes     = HASH_BYTES_MAX;
    uint8_t        *hash          = OPENSSL_malloc(HASH_BYTES_MAX);
    ECDSA_SIG      *ecdsa_sig     = NULL;
    uint8_t        *ecdsa_der     = NULL;
    uint32_t       ecdsa_der_size = 0;

    if (NULL == in_file)       return CAL_INVALID_ARGUMENT;
    if (NULL == pkey)          return CAL_INVALID_ARGUMENT;
    if (NULL == hash_type)     return CAL_INVALID_ARGUMENT;
    if (NULL == sig_buf)       return CAL_INVALID_ARGUMENT;
    if (0    == sig_buf_bytes) return CAL_INVALID_ARGUMENT;
    if (NULL == hash)          return CAL_CRYPTO_API_ERROR;

    if (CAL_SUCCESS != calculate_hash(in_file, hash_alg, hash, &hash_bytes))
        return CAL_CRYPTO_API_ERROR;

    switch (sig_fmt)
    {
        case SIG_FMT_PKCS1:
            if (1 != RSA_verify(EVP_MD_type(hash_type), hash, hash_bytes,
                                sig_buf, sig_buf_bytes, EVP_PKEY_get0_RSA(pkey)))
            {
                return CAL_INVALID_SIGNATURE;
            }
            break;

        case SIG_FMT_ECDSA:
            ecdsa_sig = ECDSA_SIG_new();
            if (NULL == ecdsa_sig)   return CAL_CRYPTO_API_ERROR;
            ECDSA_SIG_set0(ecdsa_sig,
                BN_bin2bn(sig_buf, sig_buf_bytes/2, NULL),
                BN_bin2bn(sig_buf + sig_buf_bytes/2, sig_buf_bytes/2, NULL));
            ecdsa_der_size = i2d_ECDSA_SIG(ecdsa_sig, &ecdsa_der);
            if (0 == ecdsa_der_size) return CAL_CRYPTO_API_ERROR;
            if (1 != ECDSA_verify(0 /* ignored */, hash, hash_bytes,
                                  ecdsa_der, ecdsa_der_size, EVP_PKEY_get0_EC_KEY(pkey)))
            {
                return CAL_INVALID_SIGNATURE;
            }
            break;

        default:
            return CAL_INVALID_ARGUMENT;
    }

    return CAL_SUCCESS;
}

/*--------------------------
  print_license
---------------------------*/

void print_license(void)
{
    printf("\n\nNXP License Information:\n");
    printf("----------------------------------\n");
    printf("Copyright (c) Freescale Semiconductor, Inc. 2011, 2012. All rights reserved.\n");
    printf("Copyright 2018-2019 NXP\n\n");
    printf("This software is under license from NXP\n");
    printf("By using this software you agree to the license terms provided\n");
    printf("at the time this release was downloaded from www.nxp.com\n");
    printf("\nOpenssl/SSLeay License Information:\n");
    printf("---------------------------------------\n");
    printf("This product includes software developed by the OpenSSL Project\n");
    printf("for use in the OpenSSL Toolkit (http://www.openssl.org/)\n\n");
    printf("This product includes cryptographic software written by\n");
    printf("Eric Young (eay@cryptsoft.com)\n");
    printf("This product includes cryptographic software written by\n");
    printf("Brian Gladman, Worcester, UK\n");
    printf("\nThe following is the full license text for OpenSSL, SSLeay\n");
    printf("and Brian Gladman:\n\n");
    printf("OpenSSL License\n");
    printf("---------------\n\n");
    printf("/* ====================================================================\n");
    printf(" * Copyright (c) 1998-2018 The OpenSSL Project.  All rights reserved.\n");
    printf(" *\n");
    printf(" * Redistribution and use in source and binary forms, with or without\n");
    printf(" * modification, are permitted provided that the following conditions\n");
    printf(" * are met:\n");
    printf(" *\n");
    printf(" * 1. Redistributions of source code must retain the above copyright\n");
    printf(" *    notice, this list of conditions and the following disclaimer.\n");
    printf(" *\n");
    printf(" * 2. Redistributions in binary form must reproduce the above copyright\n");
    printf(" *    notice, this list of conditions and the following disclaimer in\n");
    printf(" *    the documentation and/or other materials provided with the\n");
    printf(" *    distribution.\n");
    printf(" *\n");
    printf(" * 3. All advertising materials mentioning features or use of this\n");
    printf(" *    software must display the following acknowledgment:\n");
    printf(" *    \"This product includes software developed by the OpenSSL Project\n");
    printf(" *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)\"\n");
    printf(" *\n");
    printf(" * 4. The names \"OpenSSL Toolkit\" and \"OpenSSL Project\" must not be used to\n");
    printf(" *    endorse or promote products derived from this software without\n");
    printf(" *    prior written permission. For written permission, please contact\n");
    printf(" *    openssl-core@openssl.org.\n");
    printf(" *\n");
    printf(" * 5. Products derived from this software may not be called \"OpenSSL\"\n");
    printf(" *    nor may \"OpenSSL\" appear in their names without prior written\n");
    printf(" *    permission of the OpenSSL Project.\n");
    printf(" *\n");
    printf(" * 6. Redistributions of any form whatsoever must retain the following\n");
    printf(" *    acknowledgment:\n");
    printf(" *    \"This product includes software developed by the OpenSSL Project\n");
    printf(" *    for use in the OpenSSL Toolkit (http://www.openssl.org/)\"\n");
    printf(" *\n");
    printf(" * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY\n");
    printf(" * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n");
    printf(" * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR\n");
    printf(" * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR\n");
    printf(" * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n");
    printf(" * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT\n");
    printf(" * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n");
    printf(" * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n");
    printf(" * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,\n");
    printf(" * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n");
    printf(" * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED\n");
    printf(" * OF THE POSSIBILITY OF SUCH DAMAGE.\n");
    printf(" * ====================================================================\n");
    printf(" *\n");
    printf(" * This product includes cryptographic software written by Eric Young\n");
    printf(" * (eay@cryptsoft.com).  This product includes software written by Tim\n");
    printf(" * Hudson (tjh@cryptsoft.com).\n");
    printf(" *\n");
    printf(" */\n\n");
    printf("Original SSLeay License\n");
    printf("-----------------------\n\n");
    printf("/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)\n");
    printf(" * All rights reserved.\n");
    printf(" *\n");
    printf(" * This package is an SSL implementation written\n");
    printf(" * by Eric Young (eay@cryptsoft.com).\n");
    printf(" * The implementation was written so as to conform with Netscapes SSL.\n");
    printf(" *\n");
    printf(" * This library is free for commercial and non-commercial use as long as\n");
    printf(" * the following conditions are aheared to.  The following conditions\n");
    printf(" * apply to all code found in this distribution, be it the RC4, RSA,\n");
    printf(" * lhash, DES, etc., code; not just the SSL code.  The SSL documentation\n");
    printf(" * included with this distribution is covered by the same copyright terms\n");
    printf(" * except that the holder is Tim Hudson (tjh@cryptsoft.com).\n");
    printf(" *\n");
    printf(" * Copyright remains Eric Young's, and as such any Copyright notices in\n");
    printf(" * the code are not to be removed.\n");
    printf(" * If this package is used in a product, Eric Young should be given attribution\n");
    printf(" * as the author of the parts of the library used.\n");
    printf(" * This can be in the form of a textual message at program startup or\n");
    printf(" * in documentation (online or textual) provided with the package.\n");
    printf(" *\n");
    printf(" * Redistribution and use in source and binary forms, with or without\n");
    printf(" * modification, are permitted provided that the following conditions\n");
    printf(" * are met:\n");
    printf(" * 1. Redistributions of source code must retain the copyright\n");
    printf(" *    notice, this list of conditions and the following disclaimer.\n");
    printf(" * 2. Redistributions in binary form must reproduce the above copyright\n");
    printf(" *    notice, this list of conditions and the following disclaimer in the\n");
    printf(" *    documentation and/or other materials provided with the distribution.\n");
    printf(" * 3. All advertising materials mentioning features or use of this software\n");
    printf(" *    must display the following acknowledgement:\n");
    printf(" *    \"This product includes cryptographic software written by\n");
    printf(" *     Eric Young (eay@cryptsoft.com)\"\n");
    printf(" *    The word 'cryptographic' can be left out if the rouines from the library\n");
    printf(" *    being used are not cryptographic related :-).\n");
    printf(" * 4. If you include any Windows specific code (or a derivative thereof) from\n");
    printf(" *    the apps directory (application code) you must include an acknowledgement:\n");
    printf(" *    \"This product includes software written by Tim Hudson (tjh@cryptsoft.com)\"\n");
    printf(" *\n");
    printf(" * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND\n");
    printf(" * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n");
    printf(" * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n");
    printf(" * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE\n");
    printf(" * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n");
    printf(" * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n");
    printf(" * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n");
    printf(" * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n");
    printf(" * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n");
    printf(" * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n");
    printf(" * SUCH DAMAGE.\n");
    printf(" *\n");
    printf(" * The licence and distribution terms for any publically available version or\n");
    printf(" * derivative of this code cannot be changed.  i.e. this code cannot simply be\n");
    printf(" * copied and put under another distribution licence\n");
    printf(" * [including the GNU Public Licence.]\n");
    printf(" */\n\n");
    printf("Original Brian Gladman License\n");
    printf("------------------------------\n\n");
    printf("  /*\n");
    printf("  ---------------------------------------------------------------------------\n");
    printf("  Copyright (c) 1998-2008, Brian Gladman, Worcester, UK. All rights reserved.\n");
    printf(" \n");
    printf("  LICENSE TERMS\n");
    printf(" \n");
    printf("  The redistribution and use of this software (with or without changes)\n");
    printf("  is allowed without the payment of fees or royalties provided that:\n");
    printf(" \n");
    printf("   1. source code distributions include the above copyright notice, this\n");
    printf("      list of conditions and the following disclaimer;\n");
    printf(" \n");
    printf("   2. binary distributions include the above copyright notice, this list\n");
    printf("      of conditions and the following disclaimer in their documentation;\n");
    printf(" \n");
    printf("   3. the name of the copyright holder is not used to endorse products\n");
    printf("      built using this software without specific written permission.\n");
    printf(" \n");
    printf("  DISCLAIMER\n");
    printf(" \n");
    printf("  This software is provided 'as is' with no explicit or implied warranties\n");
    printf("  in respect of its properties, including, but not limited to, correctness\n");
    printf("  and/or fitness for purpose.\n");
    printf("  ---------------------------------------------------------------------------\n");
    printf("  Issue Date: 20/12/2007\n");
    printf(" */\n\n");
}
