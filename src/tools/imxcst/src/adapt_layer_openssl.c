/*===========================================================================*/
/**
    @file    adapt_layer_openssl.c

    @brief   Implements Code Signing Tool's Adaptation Layer API for the
             Freescale reference Code Signing Tool.  This file may be
             replaced in implementations using a Hardware Security Module
             or a client/server based infrastructure.

@verbatim
=============================================================================

              Freescale Semiconductor
        (c) Freescale Semiconductor, Inc. 2011-2015. All rights reserved.
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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ssl_wrapper.h"
#include <string.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/cms.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/rsa.h>
#include "adapt_layer.h"
#include "openssl_helper.h"
#include "pkey.h"
#if (defined _WIN32 || defined __CYGWIN__) && defined USE_APPLINK
#include <openssl/applink.c>
#endif
/*===========================================================================
                                 LOCAL MACROS
=============================================================================*/
#define MAX_CMS_DATA                4096   /**< Max bytes in CMS_ContentInfo */
#define MAX_LINE_CHARS              1024   /**< Max. chars in output line    */

/*===========================================================================
                  LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
=============================================================================*/

/*===========================================================================
                          LOCAL FUNCTION PROTOTYPES
=============================================================================*/

/** Converts hash_alg to an equivalent NID value for OpenSSL
 *
 * @param[in] hash_alg Hash digest algorithm from #hash_alg_t
 *
 * @pre hash_alg is a valid value from #hash_alg_t
 *
 * @returns Openssl NID value corresponding to a valid value for @a hash_alg,
 *          NID_undef otherwise.
 */
static int32_t
get_NID(hash_alg_t hash_alg);

/** Generate raw PKCS#1 Signature Data
 *
 * Generates a raw PKCS#1 v1.5 signature for the given data file, signer
 * certificate, and hash algorithm. The signature data is returned in
 * a buffer provided by caller.
 *
 * @param[in] in_file string containing path to file with data to sign
 *
 * @param[in] key_file string containing path to signing key
 *
 * @param[in] hash_alg hash algorithm from #hash_alg_t
 *
 * @param[out] sig_buf signature data buffer
 *
 * @param[in,out] sig_buf_bytes On input, contains size of @a sig_buf in bytes,
 *                              On output, contains size of signature in bytes.
 *
 * @pre @a in_file, @a cert_file, @a key_file, @a sig_buf and @a sig_buf_bytes
 *         must not be NULL.
 *
 * @post On success @a sig_buf is updated to hold the resulting signature and
 *       @a sig_buf_bytes is updates to hold the length of the signature in
 *       bytes
 *
 * @retval #CAL_SUCCESS API completed its task successfully
 *
 * @retval #CAL_CRYPTO_API_ERROR An Openssl related error has occured
 */
static int32_t
gen_sig_data_raw(const char *in_file,
                 const char *key_file,
                 hash_alg_t hash_alg,
                 uint8_t *sig_buf,
                 int32_t *sig_buf_bytes);

/** Generate CMS Signature Data
 *
 * Generates a CMS signature for the given data file, signer certificate, and
 * hash algorithm. The signature data is returned in a buffer provided by
 * caller.  Note that sign_data cannot be used here since that function
 * requires an input buffer as an argument.  For large files it becomes
 * unreasonable to allocate a contigous block of memory.
 *
 * @param[in] in_file string containing path to file with data to sign
 *
 * @param[in] cert_file string constaining path to signer certificate
 *
 * @param[in] hash_alg hash algorithm from #hash_alg_t
 *
 * @param[out] sig_buf signature data buffer
 *
 * @param[in,out] sig_buf_bytes On input, contains size of @a sig_buf in bytes,
 *                              On output, contains size of signature in bytes.
 *
 * @pre @a in_file, @a cert_file, @a key_file, @a sig_buf and @a sig_buf_bytes
 *         must not be NULL.
 *
 * @post On success @a sig_buf is updated to hold the resulting signature and
 *       @a sig_buf_bytes is updates to hold the length of the signature in
 *       bytes
 *
 * @retval #CAL_SUCCESS API completed its task successfully
 *
 * @retval #CAL_INVALID_ARGUMENT One of the input arguments is invalid
 *
 * @retval #CAL_CRYPTO_API_ERROR An Openssl related error has occured
 */
static int32_t
gen_sig_data_cms(const char *in_file,
                 const char *cert_file,
                 const char *key_file,
                 hash_alg_t hash_alg,
                 uint8_t *sig_buf,
                 size_t *sig_buf_bytes);

/** Copies CMS Content Info with encrypted or signature data to buffer
 *
 * @param[in] cms CMS Content Info
 *
 * @param[in] bio_in input bio
 *
 * @param[out] data_buffer address to data buffer
 *
 * @param[in] data_buffer_size max size, [out] return size
 *
 * @param[in] flags CMS Flags
 *
 * @returns CAL_SUCCESS upon success
 *
 * @returns CAL_CRYPTO_API_ERROR when openssl BIO API fail
 */
int32_t cms_to_buf(CMS_ContentInfo *cms, BIO * bio_in, uint8_t * data_buffer,
                            size_t * data_buffer_size, int32_t flags);

/** generate_dek_key
 *
 * Uses openssl API to generate a random 128 bit AES key
 *
 * @param[out] key buffer to store the key data
 *
 * @param[in] len length of the key to generate
 *
 * @post if successful the random bytes are placed into output buffer
 *
 * @pre  #openssl_initialize has been called previously
 *
 * @returns if successful function returns location CAL_SUCCESS.
 */
int32_t generate_dek_key(uint8_t * key, int32_t len);

/**  write_plaintext_dek_key
 *
 * Writes the provide DEK to the give path. It will be encrypted
 * under the certificate file if provided.
 *
 * @param[in] key input key data
 *
 * @param[in] key_bytes length of the input key
 *
 * @param[in] cert_file  certificate to encrypt the DEK
 *
 * @param[in] enc_file  destination file
 *
 * @post if successful the dek is written to the file
 *
 * @returns if successful function returns location CAL_SUCCESS.
 */
int32_t write_plaintext_dek_key(uint8_t * key, size_t key_bytes,
                        const char * cert_file, const char * enc_file);

/** encrypt_dek_key
 *
 * Uses openssl API to encrypt the key. Saves the encrypted structure to a file
 *
 * @param[in] key input key data
 *
 * @param[in] key_bytes length of the input key
 *
 * @param[in] cert filename of the RSA certificate, dek will be encrypted with
 *
 * @param[in] file encrypted data saved in the file
 *
 * @post if successful the file is created with the encrypted data
 *
 * @pre  #openssl_initialize has been called previously
 *
 * @returns if successful function returns location CAL_SUCCESS.
 */
int32_t encrypt_dek_key(uint8_t * key, size_t key_bytes,
                const char * cert_file, const char * enc_file);

/** Display error message
 *
 * Displays error message to STDERR
 *
 * @param[in] err Error string to display
 *
 * @pre  @a err is not NULL
 *
 * @post None
 */
static void
display_error(const char *err);

/*===========================================================================
                               GLOBAL VARIABLES
=============================================================================*/

/*===========================================================================
                               LOCAL FUNCTIONS
=============================================================================*/

/*--------------------------
  get_NID
---------------------------*/
int32_t
get_NID(hash_alg_t hash_alg)
{
    return OBJ_txt2nid(get_digest_name(hash_alg));
}

/*--------------------------
  gen_sig_data_raw
---------------------------*/
int32_t
gen_sig_data_raw(const char *in_file,
                 const char *key_file,
                 hash_alg_t hash_alg,
                 uint8_t *sig_buf,
                 int32_t *sig_buf_bytes)
{
    EVP_PKEY *key = NULL; /**< Ptr to read key data */
    RSA *rsa = NULL; /**< Ptr to rsa of key data */
    uint8_t *rsa_in = NULL; /**< Mem ptr for hash data of in_file */
    uint8_t *rsa_out = NULL; /**< Mem ptr for encrypted data */
    int32_t rsa_inbytes; /**< Holds the length of rsa_in buf */
    int32_t rsa_outbytes = 0; /**< Holds the length of rsa_out buf */
    int32_t key_bytes; /**< Size of key data */
    int32_t hash_nid; /**< hash id needed for RSA_sign() */
    /** Array to hold error string */
    char err_str[MAX_ERR_STR_BYTES];
    /**< Holds the return error value */
    int32_t err_value = CAL_CRYPTO_API_ERROR;

    do
    {
        /* Read key */
        key = read_private_key(key_file,
                           (pem_password_cb *)get_passcode_to_key_file,
                           key_file);
        if (!key) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Cannot open key file %s", key_file);
            display_error(err_str);
            break;
        }

        rsa = EVP_PKEY_get1_RSA(key);
        EVP_PKEY_free(key);

        if (!rsa) {
            display_error("Unable to extract RSA key for RAW PKCS#1 signature");
            break;
        }

        rsa_inbytes = HASH_BYTES_MAX;
        rsa_in = OPENSSL_malloc(HASH_BYTES_MAX);
        key_bytes = RSA_size(rsa);
        rsa_out = OPENSSL_malloc(key_bytes);

        /* Generate hash data of data from in_file */
        err_value = calculate_hash(in_file, hash_alg, rsa_in, &rsa_inbytes);
        if (err_value != CAL_SUCCESS) {
            break;
        }

        /* Compute signature.  Note: RSA_sign() adds the appropriate DER
         * encoded prefix internally.
         */
        hash_nid = get_NID(hash_alg);
        if (!RSA_sign(hash_nid, rsa_in,
                      rsa_inbytes, rsa_out,
                      (unsigned int *)&rsa_outbytes, rsa)) {
            err_value = CAL_CRYPTO_API_ERROR;
            display_error("Unable to generate signature");
            break;
        }
        else {
            err_value = CAL_SUCCESS;
        }

        /* Copy signature to sig_buf and update sig_buf_bytes */
        *sig_buf_bytes = rsa_outbytes;
        memcpy(sig_buf, rsa_out, rsa_outbytes);
    } while(0);

    if (err_value != CAL_SUCCESS) {
        ERR_print_errors_fp(stderr);
    }

    if (rsa) RSA_free(rsa);
    if (rsa_in) OPENSSL_free(rsa_in);
    if (rsa_out) OPENSSL_free(rsa_out);
    return err_value;
}

/*--------------------------
  cms_to_buf
---------------------------*/
int32_t cms_to_buf(CMS_ContentInfo *cms, BIO * bio_in, uint8_t * data_buffer,
                            size_t * data_buffer_size, int32_t flags)
{
    int32_t err_value = CAL_SUCCESS;
    BIO * bio_out = NULL;
    BUF_MEM buffer_memory;            /**< Used with BIO functions */

    buffer_memory.length = 0;
    buffer_memory.data = (char*)data_buffer;
    buffer_memory.max = *data_buffer_size;

    do {
        if (!(bio_out = BIO_new(BIO_s_mem()))) {
            display_error("Unable to allocate CMS signature result memory");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        BIO_set_mem_buf(bio_out, &buffer_memory, BIO_NOCLOSE);

        /* Convert cms to der format */
        if (!i2d_CMS_bio_stream(bio_out, cms, bio_in, flags)) {
            display_error("Unable to convert CMS signature to DER format");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Get the size of bio out in data_buffer_size */
        *data_buffer_size = BIO_ctrl_pending(bio_out);
    }while(0);

    if (bio_out)
        BIO_free(bio_out);
    return err_value;
}

/*--------------------------
  gen_sig_data_cms
---------------------------*/
int32_t
gen_sig_data_cms(const char *in_file,
                 const char *cert_file,
                 const char *key_file,
                 hash_alg_t hash_alg,
                 uint8_t *sig_buf,
                 size_t *sig_buf_bytes)
{
    BIO             *bio_in = NULL;   /**< BIO for in_file data */
    X509            *cert = NULL;     /**< Ptr to X509 certificate read data */
    EVP_PKEY        *key = NULL;      /**< Ptr to key read data */
    CMS_ContentInfo *cms = NULL;      /**< Ptr used with openssl API */
    const EVP_MD    *sign_md = NULL;  /**< Ptr to digest name */
    int32_t err_value = CAL_SUCCESS;  /**< Used for return value */
    /** Array to hold error string */
    char err_str[MAX_ERR_STR_BYTES];
    /* flags set to match Openssl command line options for generating
     *  signatures
     */
    int32_t         flags = CMS_DETACHED | CMS_NOCERTS |
                            CMS_NOSMIMECAP | CMS_BINARY;

    /* Set signature message digest alg */
    sign_md = EVP_get_digestbyname(get_digest_name(hash_alg));
    if (sign_md == NULL) {
        display_error("Invalid hash digest algorithm");
        return CAL_INVALID_ARGUMENT;
    }

    do
    {
        cert = read_certificate(cert_file);
        if (!cert) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Cannot open certificate file %s", cert_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Read key */
        key = read_private_key(key_file,
                           (pem_password_cb *)get_passcode_to_key_file,
                           key_file);
        if (!key) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Cannot open key file %s", key_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Read Data to be signed */
        if (!(bio_in = BIO_new_file(in_file, "rb"))) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Cannot open data file %s", in_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Generate CMS Signature - can only use CMS_sign if default
         * MD is used which is SHA1 */
        flags |= CMS_PARTIAL;

        cms = CMS_sign(NULL, NULL, NULL, bio_in, flags);
        if (!cms) {
            display_error("Failed to initialize CMS signature");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        if (!CMS_add1_signer(cms, cert, key, sign_md, flags)) {
            display_error("Failed to generate CMS signature");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Finalize the signature */
        if (!CMS_final(cms, bio_in, NULL, flags)) {
            display_error("Failed to finalize CMS signature");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Write CMS signature to output buffer - DER format */
        err_value = cms_to_buf(cms, bio_in, sig_buf, sig_buf_bytes, flags);
    } while(0);

    /* Print any Openssl errors */
    if (err_value != CAL_SUCCESS) {
        ERR_print_errors_fp(stderr);
    }

    /* Close everything down */
    if (cms)      CMS_ContentInfo_free(cms);
    if (cert)     X509_free(cert);
    if (key)      EVP_PKEY_free(key);
    if (bio_in)   BIO_free(bio_in);

    return err_value;
}

/*--------------------------
  gen_sig_data_ecdsa
---------------------------*/
int32_t
gen_sig_data_ecdsa(const char *in_file,
                   const char *key_file,
                   hash_alg_t hash_alg,
                   uint8_t    *sig_buf,
                   size_t     *sig_buf_bytes)
{
    BIO          *bio_in    = NULL;          /**< BIO for in_file data    */
    EVP_PKEY     *key       = NULL;          /**< Private key data        */
    uint32_t     key_size   = 0;             /**< n of bytes of key param */
    const EVP_MD *sign_md   = NULL;          /**< Digest name             */
    uint8_t      *hash      = NULL;          /**< Hash data of in_file    */
    int32_t      hash_bytes = 0;             /**< Length of hash buffer   */
    uint8_t      *sign      = NULL;          /**< Signature data in DER   */
    uint32_t     sign_bytes = 0;             /**< Length of DER signature */
    uint8_t      *r = NULL, *s = NULL;       /**< Raw signature data R&S  */
    size_t       bn_bytes = 0;               /**< Length of R,S big num   */
    ECDSA_SIG    *sign_dec  = NULL;          /**< Raw signature data R|S  */
    int32_t      err_value  = CAL_SUCCESS;   /**< Return value            */
    char         err_str[MAX_ERR_STR_BYTES]; /**< Error string            */
    const BIGNUM *sig_r, *sig_s;             /**< signature numbers defined as OpenSSL BIGNUM */

    /* Set signature message digest alg */
    sign_md = EVP_get_digestbyname(get_digest_name(hash_alg));
    if (sign_md == NULL) {
        display_error("Invalid hash digest algorithm");
        return CAL_INVALID_ARGUMENT;
    }

    do
    {
        /* Read key */
        key = read_private_key(key_file,
                               (pem_password_cb *)get_passcode_to_key_file,
                               key_file);
        if (!key) {
            snprintf(err_str, MAX_ERR_STR_BYTES,
                     "Cannot open key file %s", key_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Read Data to be signed */
        if (!(bio_in = BIO_new_file(in_file, "rb"))) {
            snprintf(err_str, MAX_ERR_STR_BYTES,
                     "Cannot open data file %s", in_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Generate hash of data from in_file */
        hash_bytes = HASH_BYTES_MAX;
        hash = OPENSSL_malloc(HASH_BYTES_MAX);

        err_value = calculate_hash(in_file, hash_alg, hash, &hash_bytes);
        if (err_value != CAL_SUCCESS) {
            break;
        }

        /* Generate ECDSA signature with DER encoding */
        sign_bytes = ECDSA_size(EVP_PKEY_get0_EC_KEY(key));
        sign = OPENSSL_malloc(sign_bytes);

        if (0 == ECDSA_sign(0 /* ignored */, hash, hash_bytes, sign, &sign_bytes, EVP_PKEY_get0_EC_KEY(key))) {
            display_error("Failed to generate ECDSA signature");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        sign_dec = d2i_ECDSA_SIG(NULL, (const uint8_t **) &sign, sign_bytes);
        if (NULL == sign_dec) {
            display_error("Failed to decode ECDSA signature");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Copy R|S to sig_buf */
        memset(sig_buf, 0, *sig_buf_bytes);

        key_size = EVP_PKEY_bits(key) >> 3;
        if (EVP_PKEY_bits(key) & 0x7) key_size += 1; /* Valid for P-521 */

        if ((key_size * 2) > *sig_buf_bytes){
            display_error("Signature buffer too small");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }
        *sig_buf_bytes = key_size * 2;

        ECDSA_SIG_get0(sign_dec, &sig_r, &sig_s);

        r = get_bn(sig_r, &bn_bytes);
        memcpy(sig_buf + (key_size - bn_bytes),
               r,
               bn_bytes);
        free(r);

        s = get_bn(sig_s, &bn_bytes);
        memcpy(sig_buf + key_size + (key_size - bn_bytes),
               s,
               bn_bytes);
        free(s);
    } while(0);

    /* Print any Openssl errors */
    if (err_value != CAL_SUCCESS) {
        ERR_print_errors_fp(stderr);
    }

    /* Close everything down */
    if (key)    EVP_PKEY_free(key);
    if (bio_in) BIO_free(bio_in);

    return err_value;
}

/*--------------------------
  error
---------------------------*/
void
display_error(const char *err)
{
    fprintf(stderr, "Error: %s\n", err);
}

/*--------------------------
  export_signature_request
---------------------------*/
int32_t export_signature_request(const char *in_file,
                                 const char *cert_file)
{
    #define WRITE_LINE()                                              \
        if (strlen(line) != fwrite(line, 1, strlen(line), sig_req)) { \
            snprintf(err_str, MAX_ERR_STR_BYTES,                      \
                     "Unable to write to file %s", SIG_REQ_FILENAME); \
            display_error(err_str);                                   \
            return CAL_CRYPTO_API_ERROR;                              \
        }

    char err_str[MAX_ERR_STR_BYTES]; /**< Used in preparing error message  */
    FILE *sig_req = NULL;            /**< Output signing request           */
    char line[MAX_LINE_CHARS];       /**< Used in preparing output message */

    sig_req = fopen(SIG_REQ_FILENAME, "a");
    if (NULL ==  sig_req) {
        snprintf(err_str, MAX_ERR_STR_BYTES,
                 "Unable to create file %s", SIG_REQ_FILENAME);
        display_error(err_str);
        return CAL_CRYPTO_API_ERROR;
    }

    snprintf(line, MAX_LINE_CHARS,
             "[Signing request]\r\n");
    WRITE_LINE();
    snprintf(line, MAX_LINE_CHARS,
             "Signing certificate = %s\r\n", cert_file);
    WRITE_LINE();
    snprintf(line, MAX_LINE_CHARS,
             "Data to be signed   = %s\r\n\r\n", in_file);
    WRITE_LINE();

    fclose(sig_req);

    return CAL_SUCCESS;
}

/*===========================================================================
                              GLOBAL FUNCTIONS
=============================================================================*/

/*--------------------------
  gen_sig_data
---------------------------*/
int32_t gen_sig_data(const char* in_file,
                     const char* cert_file,
                     hash_alg_t hash_alg,
                     sig_fmt_t sig_fmt,
                     uint8_t* sig_buf,
                     size_t *sig_buf_bytes,
                     func_mode_t mode)
{
    int32_t err = CAL_SUCCESS; /**< Used for return value */
    char *key_file;            /**< Mem ptr for key filename */

    /* Check for valid arguments */
    if ((!in_file) || (!cert_file) || (!sig_buf) || (!sig_buf_bytes)) {
        return CAL_INVALID_ARGUMENT;
    }

    if (MODE_HSM == mode)
    {
        return export_signature_request(in_file, cert_file);
    }

    /* Determine private key filename from given certificate filename */
    key_file = malloc(strlen(cert_file)+1);

    err = get_key_file(cert_file, key_file);
    if ( err != CAL_SUCCESS) {
        free(key_file);
        return CAL_FILE_NOT_FOUND;
    }

    if (SIG_FMT_PKCS1 == sig_fmt) {
        err = gen_sig_data_raw(in_file, key_file,
                               hash_alg, sig_buf, (int32_t *)sig_buf_bytes);
    }
    else if (SIG_FMT_CMS == sig_fmt) {
        err = gen_sig_data_cms(in_file, cert_file, key_file,
                               hash_alg, sig_buf, sig_buf_bytes);
    }
    else if (SIG_FMT_ECDSA == sig_fmt) {
        err = gen_sig_data_ecdsa(in_file, key_file,
                                 hash_alg, sig_buf, sig_buf_bytes);
    }
    else {
        free(key_file);
        display_error("Invalid signature format");
        return CAL_INVALID_ARGUMENT;
    }

    free(key_file);
    return err;
}

/*--------------------------
  generate_dek_key
---------------------------*/
int32_t generate_dek_key(uint8_t * key, int32_t len)
{
    if (gen_random_bytes(key, len) != CAL_SUCCESS) {
        return CAL_CRYPTO_API_ERROR;
    }

    return CAL_SUCCESS;
}

/*--------------------------
  write_plaintext_dek_key
---------------------------*/
int32_t write_plaintext_dek_key(uint8_t * key, size_t key_bytes,
                const char * cert_file, const char * enc_file)
{
    int32_t err_value = CAL_SUCCESS;  /**< Return value */
    char err_str[MAX_ERR_STR_BYTES];  /**< Used in preparing error message */
    FILE *fh = NULL;                  /**< File handle used with file api */
#ifdef DEBUG
    int32_t i = 0;                    /**< Used in for loops */
#endif

    UNUSED(cert_file);

    do {
        /* Save the buffer into enc_file */
        if ((fh = fopen(enc_file, "wb")) == NULL) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Unable to create binary file %s", enc_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }
        if (fwrite(key, 1, key_bytes, fh) !=
            key_bytes) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Unable to write to binary file %s", enc_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }
        fclose (fh);
   } while(0);

    return err_value;
}


/*--------------------------
  encrypt_dek_key
---------------------------*/
int32_t encrypt_dek_key(uint8_t * key, size_t key_bytes,
                const char * cert_file, const char * enc_file)
{
    X509            *cert = NULL;     /**< Ptr to X509 certificate read data */
    STACK_OF(X509) *recips = NULL;    /**< Ptr to X509 stack */
    CMS_ContentInfo *cms = NULL;      /**< Ptr to cms structure */
    const EVP_CIPHER *cipher = NULL;  /**< Ptr to EVP_CIPHER */
    int32_t err_value = CAL_SUCCESS;  /**< Return value */
    char err_str[MAX_ERR_STR_BYTES];  /**< Used in preparing error message */
    BIO *bio_key = NULL;              /**< Bio for the key data to encrypt */
    uint8_t * enc_buf = NULL;         /**< Ptr for encoded key data */
    FILE *fh = NULL;                  /**< File handle used with file api */
    size_t cms_info_size = MAX_CMS_DATA; /**< Size of cms content info*/
#ifdef DEBUG
    int32_t i = 0;                    /**< Used in for loops */
#endif

    do {
        /* Read the certificate from cert_file */
        cert = read_certificate(cert_file);
        if (!cert) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Cannot open certificate file %s", cert_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Create recipient STACK and add recipient cert to it */
        recips = sk_X509_new_null();

        if (!recips || !sk_X509_push(recips, cert)) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Cannot instantiate object STACK_OF(%s)", cert_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /*
         * sk_X509_pop_free will free up recipient STACK and its contents
         * so set cert to NULL so it isn't freed up twice.
         */
        cert = NULL;

        /* Instantiate correct cipher */
        if (key_bytes == (AES_KEY_LEN_128 / BYTE_SIZE_BITS))
            cipher = EVP_aes_128_cbc();
        else if (key_bytes == (AES_KEY_LEN_192 / BYTE_SIZE_BITS))
            cipher = EVP_aes_192_cbc();
        else if (key_bytes == (AES_KEY_LEN_256 / BYTE_SIZE_BITS))
            cipher = EVP_aes_256_cbc();
        if (cipher == NULL) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Invalid cipher used for encrypting key %s", enc_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Allocate memory buffer BIO for input key */
        bio_key = BIO_new_mem_buf(key, key_bytes);
        if (!bio_key) {
            display_error("Unable to allocate BIO memory");
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Encrypt content of the key with certificate */
        cms = CMS_encrypt(recips, bio_key, cipher, CMS_BINARY|CMS_STREAM);
        if (cms == NULL) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Failed to encrypt key data");
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Finalize the CMS content info structure */
        if (!CMS_final(cms, bio_key, NULL,  CMS_BINARY|CMS_STREAM)) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Failed to finalize cms data");
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Alloc mem to convert cms to binary and save it into enc_file */
        enc_buf = malloc(MAX_CMS_DATA);
        if (enc_buf == NULL) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Failed to allocate memory");
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }

        /* Copy cms info into enc_buf */
        err_value = cms_to_buf(cms, bio_key, enc_buf, &cms_info_size,
            CMS_BINARY);

        /* Save the buffer into enc_file */
        if ((fh = fopen(enc_file, "wb")) == NULL) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Unable to create binary file %s", enc_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }
        if (fwrite(enc_buf, 1, cms_info_size, fh) !=
            cms_info_size) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Unable to write to binary file %s", enc_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }
        fclose (fh);
#ifdef DEBUG
        printf("Encoded key ;");
        for(i=0; i<key_bytes; i++) {
            printf("%02x ", enc_buf[i]);
        }
        printf("\n");
#endif
    } while(0);

    if (cms)
        CMS_ContentInfo_free(cms);
    if (cert)
        X509_free(cert);
    if (recips)
        sk_X509_pop_free(recips, X509_free);
    if (bio_key)
        BIO_free(bio_key);
    return err_value;
}

/*--------------------------
  gen_auth_encrypted_data
---------------------------*/
int32_t gen_auth_encrypted_data(const char* in_file,
                     const char* out_file,
                     aead_alg_t aead_alg,
                     uint8_t *aad,
                     size_t aad_bytes,
                     uint8_t *nonce,
                     size_t nonce_bytes,
                     uint8_t *mac,
                     size_t mac_bytes,
                     size_t key_bytes,
                     const char* cert_file,
                     const char* key_file,
                     int reuse_dek)
{
    int32_t err_value = CAL_SUCCESS;         /**< status of function calls */
    char err_str[MAX_ERR_STR_BYTES];         /**< Array to hold error string */
    static uint8_t key[MAX_AES_KEY_LENGTH];  /**< Buffer for random key */
    static uint8_t key_init_done = 0;        /**< Status of key initialization */
    FILE *fh = NULL;                         /**< Used with files */
    size_t file_size;                        /**< Size of in_file */
    unsigned char *plaintext = NULL;                /**< Array to read file data */
    int32_t bytes_read;
#ifdef DEBUG
    int32_t i;                                        /**< used in for loops */
#endif

    do {
        if (AES_CCM == aead_alg) { /* HAB4 */
            /* Generate Nonce */
            err_value = gen_random_bytes((uint8_t*)nonce, nonce_bytes);
            if (err_value != CAL_SUCCESS) {
                snprintf(err_str, MAX_ERR_STR_BYTES-1,
                            "Failed to get nonce");
                display_error(err_str);
                err_value = CAL_CRYPTO_API_ERROR;
                break;
            }
        }
#ifdef DEBUG
        printf("nonce bytes: ");
        for(i=0; i<nonce_bytes; i++) {
            printf("%02x ", nonce[i]);
        }
        printf("\n");
#endif
        if (0 == key_init_done) {
            if (reuse_dek) {
                fh = fopen(key_file, "rb");
                if (fh == NULL) {
                    snprintf(err_str, MAX_ERR_STR_BYTES-1,
                        "Unable to open dek file %s", key_file);
                    display_error(err_str);
                    err_value = CAL_FILE_NOT_FOUND;
                    break;
                }
                /* Read encrypted data into input_buffer */
                bytes_read = fread(key, 1, key_bytes, fh);
                if (bytes_read == 0) {
                    snprintf(err_str, MAX_ERR_STR_BYTES-1,
                        "Cannot read file %s", key_file);
                    display_error(err_str);
                    err_value = CAL_FILE_NOT_FOUND;
                    fclose(fh);
                    break;
                }
                fclose(fh);
            }
            else {
                /* Generate random aes key to use it for encrypting data */
                    err_value = generate_dek_key(key, key_bytes);
                    if (err_value) {
                        snprintf(err_str, MAX_ERR_STR_BYTES-1,
                                    "Failed to generate random key");
                        display_error(err_str);
                        err_value = CAL_CRYPTO_API_ERROR;
                        break;
                    }
            }

#ifdef DEBUG
            printf("random key : ");
            for (i=0; i<key_bytes; i++) {
                printf("%02x ", key[i]);
            }
            printf("\n");
#endif
            if (cert_file!=NULL) {
                /* Encrypt key using cert file and save it in the key_file */
                err_value = encrypt_dek_key(key, key_bytes, cert_file, key_file);
                if (err_value) {
                    snprintf(err_str, MAX_ERR_STR_BYTES-1,
                            "Failed to encrypt and save key");
                    display_error(err_str);
                    err_value = CAL_CRYPTO_API_ERROR;
                    break;
                }
            } else {
                /* Save key in the key_file */
                err_value = write_plaintext_dek_key(key, key_bytes, cert_file, key_file);
                if (err_value) {
                    snprintf(err_str, MAX_ERR_STR_BYTES-1,
                            "Failed to save key");
                    display_error(err_str);
                    err_value = CAL_CRYPTO_API_ERROR;
                    break;
                }
            }

            key_init_done = 1;
        }

        /* Get the size of in_file */
        fh = fopen(in_file, "rb");
        if (fh == NULL) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                     "Unable to open binary file %s", in_file);
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }
        fseek(fh, 0, SEEK_END);
        file_size = ftell(fh);
        plaintext = (unsigned char*)malloc(file_size);;
        if (plaintext == NULL) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                         "Not enough allocated memory" );
            display_error(err_str);
            err_value = CAL_CRYPTO_API_ERROR;
            break;
        }
        fclose(fh);

        fh = fopen(in_file, "rb");
        if (fh == NULL) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                         "Cannot open file %s", in_file);
            display_error(err_str);
            err_value = CAL_FILE_NOT_FOUND;
            break;
        }

        /* Read encrypted data into input_buffer */
        bytes_read = fread(plaintext, 1, file_size, fh);
        fclose(fh);
        /* Reached EOF? */
        if (bytes_read == 0) {
            snprintf(err_str, MAX_ERR_STR_BYTES-1,
                         "Cannot read file %s", out_file);
            display_error(err_str);
            err_value = CAL_FILE_NOT_FOUND;
           break;
        }

        if (AES_CCM == aead_alg) { /* HAB4 */
            err_value = encryptccm(plaintext, file_size, aad, aad_bytes,
                                key, key_bytes, nonce, nonce_bytes, out_file,
                                mac, mac_bytes, &err_value, err_str);
        }
        else if (AES_CBC == aead_alg) { /* AHAB */
            err_value = encryptcbc(plaintext, file_size, key, key_bytes, nonce,
                                   out_file, &err_value, err_str);
        }
        else {
            err_value = CAL_INVALID_ARGUMENT;
        }
        if (err_value == CAL_NO_CRYPTO_API_ERROR) {
            printf("Encryption not enabled\n");
            break;
        }
    } while(0);

    free(plaintext);

    /* Clean up */
    return err_value;
}
