#ifndef PKEY_H
#define PKEY_H
/*===========================================================================*/
/**
    @file    pkey.h

    @brief   CST private key and password provider API

@verbatim
=============================================================================

              Freescale Semiconductor
        (c) Freescale Semiconductor, Inc. 2011,2012. All rights reserved.

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

/*===========================================================================
                              CONSTANTS
=============================================================================*/

/*===========================================================================
                                MACROS
=============================================================================*/

/*===========================================================================
                                ENUMS
=============================================================================*/

/*===========================================================================
                    STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

/*===========================================================================
                     GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/*===========================================================================
                         FUNCTION PROTOTYPES
=============================================================================*/
#ifdef __cplusplus
extern "C" {
#endif

/** get_passcode_to_key_file
 *
 * @par Purpose
 *
 * Callback to gte password for the encrypted key file. Default behavior
 * can be overridden by customers by linking with their own implementation
 * of libpw.a and this function
 *
 * @par Operation
 *
 * @param[out] buf,  return buffer for the password
 *
 * @param[in] size,  size of the buf in bytes
 *
 * @param[in] rwflag,  not used
 *
 * @param[in] userdata,  filename for a public key used in the CSF
 *
 * @retval returns size of password string
 */
int get_passcode_to_key_file(char *buf, int size, int rwflag, void *userdata);

/** get_key_file
 *
 * @par Purpose
 *
 * This API extracts private key filename using cert filename. This API is
 * moved to libpw to allow customers to change its implementation to better
 * suit their needs.
 *
 * @par Operation
 *
 * @param[in] cert_file,  filename for certificate
 *
 * @param[out] key_file,  filename for private key for the input certificate
 *
 * @retval SUCCESS
 */
int32_t get_key_file(const char* cert_file, char* key_file);

#ifdef __cplusplus
}
#endif

#endif /* PKEY_H */
