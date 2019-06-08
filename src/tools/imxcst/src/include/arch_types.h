#ifndef ARCH_TYPES_H
#define ARCH_TYPES_H
/*===========================================================================*/
/**
    @file    arch_types.h

    @brief   Boot architectures interface

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
#include "hab_types.h"
#include "ahab_types.h"

/*===========================================================================
                                MACROS
=============================================================================*/

/* Maximums supported by the different architectures */
#define MAX_CERTIFICATES_ALLOWED  4    /**< Max number of X.509 certs */
#define MAX_SRK_TABLE_BYTES       3072 /**< Maximum bytes for SRK table */

/* HAB4/AHAB SRK Table definitions */
#define SRK_TABLE_HEADER_BYTES    4    /**< Number of bytes in table header */
#define SRK_KEY_HEADER_BYTES      12   /**< Number of bytes in key header */

/* Missing define in container.h */
#define SRK_RSA3072               0x6

/*===========================================================================
                                TYPEDEFS
=============================================================================*/
typedef enum tgt_e
{
    TGT_UNDEF = 0, /**< Undefined target */
    TGT_HAB,       /**< HAB target       */
    TGT_AHAB       /**< AHAB target      */
} tgt_t;

typedef enum srk_set_e
{
    SRK_SET_UNDEF = 0, /**< Undefined SRK set */
    SRK_SET_NXP,       /**< NXP SRK set       */
    SRK_SET_OEM,       /**< OEM SRK set       */
} srk_set_t;

#endif /* ARCH_TYPES_H */
