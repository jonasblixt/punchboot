/*===========================================================================*/
/**
    @file    ssl_wrapper.c

    @brief   Implements Code Signing Tool's SSL Wrapper API for the
             Freescale reference Code Signing Tool.  .

@verbatim
=============================================================================

              Freescale Semiconductor
        (c) Freescale Semiconductor, Inc. 2011-2015. All rights reserved.
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

=============================================================================*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ssl_wrapper.h"

void handle_errors(char * str,  int32_t *err_value, char *err_str) {
    snprintf(err_str, MAX_ERR_STR_BYTES-1, "%s", str);
    *err_value = CAL_CRYPTO_API_ERROR;
}

int32_t encryptccm(unsigned char *plaintext, int plaintext_len, unsigned char *aad,
    int aad_len, unsigned char *key, int key_len, unsigned char *iv, int iv_len,
    const char * out_file, unsigned char *tag, int tag_len, int32_t *err_value,
    char *err_str) {

#ifdef REMOVE_ENCRYPTION
    UNUSED(plaintext);
    UNUSED(plaintext_len);
    UNUSED(aad);
    UNUSED(aad_len);
    UNUSED(key);
    UNUSED(key_len);
    UNUSED(iv);
    UNUSED(iv_len);
    UNUSED(out_file);
    UNUSED(tag);
    UNUSED(tag_len);
    UNUSED(err_value);
    UNUSED(err_str);

    return CAL_NO_CRYPTO_API_ERROR;
#else
    EVP_CIPHER_CTX *ctx;

    int len;
    int ciphertext_len;

    unsigned char *ciphertext = NULL;
    ciphertext = (unsigned char *)malloc(plaintext_len + EVP_MAX_BLOCK_LENGTH);
    if (NULL == ciphertext) {
        handle_errors("Failed to allocate memory for encrypted data",
                      err_value, err_str);
        return *err_value;
    }

    FILE *fho = NULL;
    int err = 0;
    do{
        /* Create and initialise the context */
        if (!(ctx = EVP_CIPHER_CTX_new())) {
            handle_errors("Failed to allocate ccm context structure",
                          err_value, err_str);
            break;
        }

        /* Initialise the encryption operation. */
        switch(key_len) {
            case 16:
                err = EVP_EncryptInit_ex(ctx, EVP_aes_128_ccm(), NULL, NULL, NULL);
                break;
            case 24:
                err = EVP_EncryptInit_ex(ctx, EVP_aes_192_ccm(), NULL, NULL, NULL);
                break;
            case 32:
                err = EVP_EncryptInit_ex(ctx, EVP_aes_256_ccm(), NULL, NULL, NULL);
                break;
            default:
                handle_errors("Failed to allocate ccm context structure",
                              err_value, err_str);
                free(ciphertext);
                return *err_value;
        }

        if (err != 1) {
            handle_errors("Failed to initialize ccm context structure",
                          err_value, err_str);
            break;
        }

        /*
         * Setting IV len to 7. Not strictly necessary as this is the default
         * but shown here for the purposes of this example
         */
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_IVLEN, iv_len, NULL)) {
            handle_errors("Failed to initialize IV", err_value, err_str);
            break;
        }

        /* Set tag length */
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_TAG, tag_len, NULL);

        /* Initialise key and IV */
        if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) {
            handle_errors("Failed to intialize key", err_value, err_str);
            break;
        }

        /* Provide the total plaintext length */
        if (1 != EVP_EncryptUpdate(ctx, NULL, &len, NULL, plaintext_len)) {
            handle_errors("Failed to initialize length parameter", err_value, err_str);
            break;
        }

        /*
         * Provide the message to be encrypted, and obtain the encrypted output.
         * EVP_EncryptUpdate can only be called once for this
         */
        if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
            handle_errors("Failed to encrypt", err_value, err_str);
            break;
        }
        ciphertext_len = len;

        /* Open out_file for writing */
        fho = fopen(out_file, "wb");
        if (fho == NULL) {
            handle_errors("Cannot open file", err_value, err_str);
            break;
        }

        /* Write encrypted data to out file */
        if (fwrite(ciphertext, 1, ciphertext_len, fho) != ciphertext_len) {
            handle_errors("Cannot write file", err_value, err_str);
            break;
        }

        /*
         * Finalise the encryption. Normally ciphertext bytes may be written at
         * this stage, but this does not occur in CCM mode
         */
        if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
            handle_errors("Failed to finalize", err_value, err_str);
            break;
        }
        ciphertext_len += len;

        /* Get the tag */
        if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_GET_TAG, 16, tag)) {
            handle_errors("Failed to get tag", err_value, err_str);
            break;
        }

    } while(0);

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    if (fho) {
        fclose(fho);
    }

    free(ciphertext);

    return *err_value;
#endif
}

int32_t encryptcbc(unsigned char *plaintext, int plaintext_len, unsigned char *key,
    int key_len, unsigned char *iv, const char * out_file, int32_t *err_value, char *err_str)
{
#ifdef REMOVE_ENCRYPTION
    return CAL_NO_CRYPTO_API_ERROR;
#else
    EVP_CIPHER_CTX *ctx;
    int len, ciphertext_len;
    unsigned char *ciphertext = NULL;
    ciphertext = (unsigned char *)malloc(plaintext_len);
    if (NULL == ciphertext) {
        handle_errors("Failed to allocate memory for encrypted data",
                      err_value, err_str);
        return *err_value;
    }

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        handle_errors("Fail to allocate AES-CBC context", err_value, err_str);
        free(ciphertext);
        return *err_value;
    }

    do {
        /* Initialise the encryption operation */
        const EVP_CIPHER *cipher;
        switch(key_len) {
            case 16:
                cipher = EVP_aes_128_cbc();
                break;
            case 24:
                cipher = EVP_aes_192_cbc();
                break;
            case 32:
                cipher = EVP_aes_256_cbc();
                break;
            default:
                handle_errors("Invalid key length for AES-CBC operation", err_value, err_str);
                free(ciphertext);
                return *err_value;
        }

        if(1 != EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv)) {
            handle_errors("Fail to initialise AES-CBC operation", err_value, err_str);
            break;
        }

        /*
         * If the pad parameter is zero then no padding is performed, the total
         * amount of data encrypted or decrypted must then be a multiple of the
         * block size or an error will occur.
         */
        if (1 != EVP_CIPHER_CTX_set_padding(ctx, 0)) {
            handle_errors("Fail to disable padding", err_value, err_str);
            break;
        }

        if (0 != (plaintext_len % AES_BLOCK_BYTES)) {
            handle_errors("Amount of data not multiple of AES block size (128 bits)", err_value, err_str);
            break;
        }

        /* Provide the message and get the encrypted data */
        if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {
            handle_errors("Fail to encrypt with AES-CBC", err_value, err_str);
            break;
        }
        ciphertext_len = len;

        /* Finalise the encryption. Further ciphertext bytes may be written */
        if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
            handle_errors("Fail to finalise AES-CBC encryption", err_value, err_str);
            break;
        }
        ciphertext_len += len;

        /* Write encrypted data to out file */
        FILE *out = fopen(out_file, "wb");
        if (NULL == out) {
            handle_errors("Cannot open temporary out file during AES-CBC operation", err_value, err_str);
            break;
        }

        if (fwrite(ciphertext, 1, ciphertext_len, out) != ciphertext_len) {
            handle_errors("Fail to write temporary out file during AES-CBC operation", err_value, err_str);
            break;
        }

        fclose(out);

    } while(0);

    EVP_CIPHER_CTX_free(ctx);

    free(ciphertext);

    return *err_value;
#endif
}
