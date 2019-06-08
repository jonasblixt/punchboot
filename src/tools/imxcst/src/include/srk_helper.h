#ifndef SRK_HELPER_H
#define SRK_HELPER_H
/*===========================================================================*/
/**
    @file    srk_helper.h

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
#include "arch_types.h"
#include "openssl_helper.h"

/*===========================================================================
                    STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

/* SRK table entry */
typedef struct srk_entry
{
    uint8_t *entry;      /**< Contains key data */
    size_t entry_bytes;  /**< Size of entry in bytes */
} srk_entry_t;

/*===========================================================================
                         FUNCTION PROTOTYPES
=============================================================================*/
#ifdef __cplusplus
extern "C" {
#endif

/** Generate SRK table key entry
 *
 * This function builds a PKCS#1 public key data structure as defined by
 * the HAB4 SIS from the given X.509 key data.
 *
 * @param[in] target Define which component is targeted, HAB4 or AHAB
 *
 * @param[in] pkey Pointer OpenSSL public key data structure
 *
 * @param[in] srk Pointer to a SRK table entry data structure
 *
 * @param[in] ca_flag If set this indicates key is from a CA certificate
 *
 * @param[in] sd_alg_str Define which signature hash algorithm will be used
 *                       in conjunction with the given key
 *
 * @pre @a pkey and @a srk must not be NULL
 *
 * @pre The data lacated at @a srk->entry follows the PKCS#1 key data
 *      format described in the HAB4 SIS.
 *
 * @post if successful, @a srk->entry contains the public key data and
 *       and srk->entry_bytes is updated.
 *
 * @returns #CST_SUCCESS if successful, #CST_FAILURE otherwise
 */
void
srk_entry_pkcs1(tgt_t target,
                EVP_PKEY *pkey,
                srk_entry_t *srk,
                bool ca_flag,
                const char *sd_alg_str);

/** Generate SRK table key entry
 *
 * This function builds an EC public key data structure as defined by
 * the AHAB SIS from the given X.509 key data.
 *
 * @param[in] target Define which component is targeted, HAB4 or AHAB
 *
 * @param[in] pkey Pointer OpenSSL public key data structure
 *
 * @param[in] srk Pointer to a SRK table entry data structure
 *
 * @param[in] ca_flag If set this indicates key is from a CA certificate
 *
 * @param[in] sd_alg_str Define which signature hash algorithm will be used
 *                       in conjunction with the given key
 *
 * @pre @a pkey and @a srk must not be NULL
 *
 * @pre The data lacated at @a srk->entry follows the EC key data
 *      format described in the AHAB SIS.
 *
 * @post if successful, @a srk->entry contains the public key data and
 *       and srk->entry_bytes is updated.
 *
 * @returns #CST_SUCCESS if successful, #CST_FAILURE otherwise
 */
void
srk_entry_ec(tgt_t target,
             EVP_PKEY *pkey,
             srk_entry_t *srk,
             bool ca_flag,
             const char *sd_alg_str);

/** Converts digest algorithm string to encoded tag value
 *
 * @param[in] digest_alg Case insensitive string containing "sha1", "sha256",
 *            "sha384" or "sha512"
 *
 * @pre @a digest_alg is not NULL
 *
 * @returns encoded digest value based on given @a digest_alg string,
 *          otherwise 0 is returned if @a digest_alg is not supported
 *
 */
uint32_t
digest_alg_tag(const char *digest_alg);

/** Checks for a valid signature digest command line argument and converts
 *  @a alg_str to a corresponding integer value.
 *
 * @param[in] alg_str Character string containing the digest algorithm
 *
 * @pre  @a alg_str is not NULL
 *
 * @remark If @a alg_str contains unknown algorithm string an error is
 *         displayed to STDOUT and the program exits.
 *
 * @retval #HAB_ALG_SHA256 if @a alg_str is "sha256",
 *
 * @retval #HAB_ALG_SHA384 if @a alg_str is "sha384",
 *
 * @retval #HAB_ALG_SHA512 if @a alg_str is "sha512".
 */
uint32_t
check_sign_digest_alg(const char *alg_str);

#ifdef __cplusplus
}
#endif

#endif /* SRK_HELPER_H */
