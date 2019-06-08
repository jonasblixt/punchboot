#ifndef __OPENSSL_HELPER_H
#define __OPENSSL_HELPER_H
/*===========================================================================*/
/**
    @file    openssl_helper.h

    @brief   Provide helper functions to ease openssl tasks and also defines
             common macros that can used across different tools

@verbatim
=============================================================================

              Freescale Semiconductor
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
#include "adapt_layer.h"
#include <openssl/bn.h>
#include <openssl/bio.h>
#include <openssl/pem.h>

/*===========================================================================
                                 CONSTANTS
=============================================================================*/

#define TRUE                      1 /**< Success val returned by functions */
#define FALSE                     0 /**< Failure val returned by functions */

#define X509_UTCTIME_STRING_BYTES 13 /**< Expected length of validity period
                                       *   strings in X.509 certificates using
                                       *   UTCTime format
                                       */
#define X509_GENTIME_STRING_BYTES 15 /**< Expected length of validity period
                                       *   strings in X.509 certificates using
                                       *   Generalized Time format
                                       */
#define PEM_FILE_EXTENSION        ".pem"   /* PEM file extention */
#define PEM_FILE_EXTENSION_BYTES  4        /* Length of pem extention */

/* Message digest string definitions */
#define HASH_ALG_SHA1             "sha1"   /**< String macro for sha1 */
#define HASH_ALG_SHA256           "sha256" /**< String macro for sha256 */
#define HASH_ALG_SHA384           "sha384" /**< String macro for sha384 */
#define HASH_ALG_SHA512           "sha512" /**< String macro for sha512 */
#define HASH_ALG_INVALID          "null"   /**< String macro for invalid hash */

/* Message digest length definitions */
#define HASH_BYTES_SHA1           20   /**< Size of SHA1 output bytes */
#define HASH_BYTES_SHA256         32   /**< Size of SHA256 output bytes */
#define HASH_BYTES_SHA384         48   /**< Size of SHA384 output bytes */
#define HASH_BYTES_SHA512         64   /**< Size of SHA512 output bytes */
#define HASH_BYTES_MAX            HASH_BYTES_SHA512

/* X509 certificate definitions */
#define X509_USR_CERT             0x0 /**< User certificate */
#define X509_CA_CERT              0x1 /**< CA certificate */

/*===========================================================================
                                 CONSTANTS
=============================================================================*/

/** Extracts a byte from a given 32 bit word value
 *
 * @param [in] val value to extract byte from
 *
 * @param [in] bit_shift Number of bits to shift @a val left before
 *                       extracting the least significant byte
 *
 * @returns the least significant byte after shifting @a val to the left
 *          by @a bit_shift bits
 */
#define EXTRACT_BYTE(val, bit_shift) \
    (((val) >> (bit_shift)) & 0xFF)

/*============================================================================
                                      ENUMS
=============================================================================*/

typedef enum cst_status
{
    CST_FAILURE = FALSE,
    CST_SUCCESS = TRUE
} cst_status_t;

/*============================================================================
                           STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

/*============================================================================
                           GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/*===========================================================================
                               OPENSSL 1.0.2 SUPPORT
=============================================================================*/

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)

#define OPENSSL_malloc_init CRYPTO_malloc_init
#define X509_get0_notBefore X509_get_notBefore
#define X509_get0_notAfter  X509_get_notAfter

void
ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr, const BIGNUM **ps);

int
ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s);

void
EVP_MD_CTX_free(EVP_MD_CTX *ctx);

EVP_MD_CTX *
EVP_MD_CTX_new(void);

EC_KEY *
EVP_PKEY_get0_EC_KEY(EVP_PKEY *pkey);

RSA *
EVP_PKEY_get0_RSA(EVP_PKEY *pkey);

void
RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d);

#endif

/*============================================================================
                                FUNCTION PROTOTYPES
=============================================================================*/

/** openssl_initialize
 *
 * Initializes the openssl library.  This function must be called once
 * by the program using any of the openssl helper functions
 *
 */
extern void
openssl_initialize();

/** Computes hash digest
 *
 * Calls openssl API to generate hash for the given data in buf.
 *
 * @param[in] buf, binary data for hashing
 *
 * @param[in] msg_bytes, size in bytes for binary data
 *
 * @param[in] hash_alg, character string containing hash algorithm,
 *                      "sha1" or "sha256"
 *
 * @param[out] hash_bytes, size of digest result in bytes
 *
 * @pre  #openssl_initialize has been called previously
 *
 * @pre  @a buf and @a hash_alg are not NULL.
 *
 * @post It is the responsibilty of the caller to free the memory allocated by
 *       this function holding the computed hash result.
 *
 * @returns The location of the digest result if successful, NULL otherwise
 */
extern uint8_t *
generate_hash(const uint8_t *buf, size_t msg_bytes, const char *hash_alg,
              size_t *hash_bytes);

/** get_bn
 *
 * Extracts data from an openssl BIGNUM type to a byte array.  Used
 * for extracting certificate data such as an RSA public key modulus.
 *
 * @param[in] a      BIG_NUM structure
 *
 * @param[out] bytes size of resulting byte array
 *
 * @pre @a a and @a bytes are not NULL
 *
 * @post It is the responsibilty of the caller to free the memory allocated by
 *       this function holding the big number result.
 *
 * @returns location of resulting byte array or NULL if failed to alloc mem.
 */
extern uint8_t*
get_bn(const BIGNUM *a, size_t *bytes);


/** sign_data
 *
 * Signs a data buffer with a given private key
 *
 * @param[in] skey       signer private key
 *
 * @param[in] bptr       location of data buffer to digitally sign
 *
 * @param[in] hash_alg   hash digest algorithm
 *
 * @param[out] sig_bytes size of resulting signature buffer
 *
 * @pre  #openssl_initialize has been called previously
 *
 * @post It is the responsibilty of the caller to free the memory allocated by
 *       this function holding the signature  result.
 *
 * @returns if successful returns location of resulting byte array otherwise
 * NULL.
 */
extern uint8_t*
sign_data(const EVP_PKEY *skey, const BUF_MEM *bptr, hash_alg_t hash_alg,
          size_t *sig_bytes);

/** read_certificate
 *
 * Read X.509 certificate data from given certificate file
 *
 * @param[in] filename    filename of certificate file
 *
 * @post if successful the contents of the certificate are extracted to X509
 * object.
 *
 * @pre  #openssl_initialize has been called previously
 *
 * @post caller is responsible for releasing X.509 certificate memory.
 *
 * @returns if successful function returns location of X509 object
 *   otherwise NULL.
 */
extern X509*
read_certificate(const char* filename);

/** get_der_encoded_certificate_data
 *
 * Read X.509 certificate data from given certificate file and calls openssl
 * to encode X509 to DER format and returns DER formatted data located at
 * @derder.
 *
 * @param[in] filename    filename, function will work with both PEM and DER
 *                        input certificate files.
 *
 * @param[out] der        address to write der data
 *
 * @post if successful the contents of the certificate are written at address
 * @a der.
 *
 * @pre  #openssl_initialize has been called previously
 *
 * @post caller is responsible for releasing memory location returned in @a der
 *
 * @returns if successful function returns number of bytes written at address
 * @a der, 0 otherwise.
 */
extern int32_t get_der_encoded_certificate_data(const char* filename,
                                           uint8_t ** der);

/** read_private_key
 *
 * Uses openssl API to read private key from given certificate file
 *
 * @param[in] filename    filename of key file
 *
 * @param[in] password_cb callback fn to provide password for keyfile
 *            see openssl's pem.h for callback'e prototype
 *
 * @param[in] password    password for keyfile
 *
 * @post if successful the contents of the private key are extracted to
 * EVP_PKEY object.
 *
 * @pre  #openssl_initialize has been called previously
 *
 * @post caller is responsible for releasing the private key memory.
 *
 * @returns if successful function returns location of EVP_PKEY object
 *   otherwise NULL.
 */
extern EVP_PKEY*
read_private_key(const char *filename, pem_password_cb *password_cb,
                 const char *password);

/** seed_prng
 *
 * Calls openssl API to seed prng to given bytes randomness
 *
 * @param[in] bytes   bytes to randomize the seed
 *
 * @pre  None
 *
 * @post None
 */
uint32_t seed_prng(uint32_t bytes);

/** gen_random_bytes
 *
 * Generates random bytes using openssl RAND_bytes
 *
 * @param[out] buf    buf to return the random bytes
 *
 * @param[in] bytes   size of the buf in bytes and number of random bytes
 *                    to generate
 *
 * @pre  None
 *
 * @post None
 */
int32_t gen_random_bytes(uint8_t *buf, size_t bytes);

/** Diplays program license information to stdout
 *
 * @pre  None
 *
 * @post None
 */
extern void
print_license(void);

/** Diplays program version information to stdout
 *
 * @pre  None
 *
 * @post None
 */
extern void
print_version(void);

#endif /* __OPENSSL_HELPER_H */
