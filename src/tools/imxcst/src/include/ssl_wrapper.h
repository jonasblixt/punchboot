#ifndef SSL_WRAPPER_H
#define SSL_WRAPPER_H
/*===========================================================================

    @file    ssl_wrapper.h

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

===========================================================================*/

/*===========================================================================
                            INCLUDE FILES
=============================================================================*/
#ifndef REMOVE_ENCRYPTION
#include <openssl/evp.h>
#endif
#include <adapt_layer.h>

/*===========================================================================
                                MACROS
=============================================================================*/
#define MAX_ERR_STR_BYTES (120) /**< Max. error string bytes */

/*===========================================================================
                         FUNCTION PROTOTYPES
=============================================================================*/
#ifdef __cplusplus
extern "C" {
#endif

void handle_errors(char * str,  int32_t *err_value, char *err_str);

int32_t encryptccm(unsigned char *plaintext, int plaintext_len, unsigned char *aad,
                   int aad_len, unsigned char *key, int key_len, unsigned char *iv,
                   int iv_len, const char * out_file, unsigned char *tag, int tag_len,
                   int32_t *err_value, char *err_str);

int32_t encryptcbc(unsigned char *plaintext, int plaintext_len, unsigned char *key,
    int key_len, unsigned char *iv, const char *out_file, int32_t *err_value, char *err_str);

#ifdef __cplusplus
}
#endif

#endif /* SSL_WRAPPER_H  */
