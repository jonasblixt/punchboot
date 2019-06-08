#ifndef HAB_CMD_H
#define HAB_CMD_H
/*===========================================================================*/
/**
    @file    hab_cmd.h

    @brief

@verbatim
=============================================================================

              Freescale Semiconductor
        (c) Freescale Semiconductor, Inc. 2007, 2008. All rights reserved.

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
/*===========================================================================
                                   CONSTANTS
=============================================================================*/

#define HDR_BYTES 4  /* cannot use sizeof(hab_hdr_t) in preprocessor */

/*===========================================================================
                                     MACROS
=============================================================================*/
/*
 *    Helper macros
 */
#define HAB_CMD_UNS     0xff

#define DEFAULT_IMG_KEY_IDX     2

#define GEN_MASK(width)                         \
    ((1UL << (width)) - 1)

#define GEN_FIELD(f, width, shift)              \
    (((f) & GEN_MASK(width)) << (shift))

#define PACK_UINT32(a, b, c, d)                          \
    ((uint32_t) ( (((uint32_t)(a) & 0xFF) << 24)          \
                  |(((uint32_t)(b) & 0xFF) << 16)         \
                  |(((uint32_t)(c) & 0xFF) << 8)          \
                  |(((uint32_t)(d) & 0xFF)) ) )

#define EXPAND_UINT32(w)                                                \
    (uint8_t)((w)>>24), (uint8_t)((w)>>16), (uint8_t)((w)>>8), (uint8_t)(w)

#define HDR(tag, bytes, par)                                            \
    (uint8_t)(tag), (uint8_t)((bytes)>>8), (uint8_t)(bytes), (uint8_t)(par)

#define HAB_VER(maj, min)                                       \
    (GEN_FIELD((maj), HAB_VER_MAJ_WIDTH, HAB_VER_MAJ_SHIFT)     \
     | GEN_FIELD((min), HAB_VER_MIN_WIDTH, HAB_VER_MIN_SHIFT))

/*
 *    CSF header
 */

#define CSF_HDR(bytes, HABVER)                  \
    HDR(HAB_TAG_CSF, (bytes), HABVER)


/*
 *    DCD  header
 */

#define DCD_HDR(bytes, HABVER)                  \
    HDR(HAB_TAG_DCD, (bytes), HABVER)

/*
 *   IVT  header (goes in the struct's hab_hdr_t field, not a byte array)
 */
#define IVT_HDR(bytes, HABVER)                  \
    {HAB_TAG_IVT, {(uint8_t)((bytes)>>8), (uint8_t)(bytes)}, HABVER}

/*
 *    Write Data
 */

#define WRT_DAT(flags, bytes, address, val_msk)                         \
    HDR(HAB_CMD_WRT_DAT, WRT_DAT_BYTES, WRT_DAT_PAR((flags), (bytes))), \
        EXPAND_UINT32(address),                                         \
        EXPAND_UINT32(val_msk)

#define WRT_DAT_BYTES 12

#define MULTI_WRT_DAT(flags, bytes, address1, val_msk1, address2,       \
                            val_msk2, address3, val_msk3)               \
    HDR(HAB_CMD_WRT_DAT, MULTI_WRT_DAT_BYTES, WRT_DAT_PAR((flags), (bytes))), \
        EXPAND_UINT32(address1),                                        \
        EXPAND_UINT32(val_msk1),                                        \
        EXPAND_UINT32(address2),                                        \
        EXPAND_UINT32(val_msk2),                                        \
        EXPAND_UINT32(address3),                                        \
        EXPAND_UINT32(val_msk3)

#define MULTI_WRT_DAT_BYTES 28

#define WRT_DAT_PAR(flags, bytes)               \
    (GEN_FIELD((flags),                         \
               HAB_CMD_WRT_DAT_FLAGS_WIDTH,     \
               HAB_CMD_WRT_DAT_FLAGS_SHIFT)     \
     | GEN_FIELD((bytes),                       \
                 HAB_CMD_WRT_DAT_BYTES_WIDTH,   \
                 HAB_CMD_WRT_DAT_BYTES_SHIFT))

/*
 *    Check Data (forever)
 */

#define CHK_DAT_FOREVER(flags, bytes, address, mask)                    \
    HDR(HAB_CMD_CHK_DAT, CHK_DAT_FOREVER_BYTES, WRT_DAT_PAR((flags), (bytes))), \
        EXPAND_UINT32(address),                                         \
        EXPAND_UINT32(mask)

#define CHK_DAT_FOREVER_BYTES 12

/*
 *    Check Data (polled)
 */
#define HAB_CMD_CHK_DAT_COUNT 100

#define CHK_DAT(flags, bytes, address, mask, count)                     \
    HDR(HAB_CMD_CHK_DAT, CHK_DAT_BYTES, WRT_DAT_PAR((flags), (bytes))), \
        EXPAND_UINT32(address),                                         \
        EXPAND_UINT32(mask),                                            \
        EXPAND_UINT32(count)

#define CHK_DAT_BYTES 16

/*
 *    Set (generic - used internally only, or to generate invalid commands)
 */

#define SET(bytes, itm, value)                  \
    HDR(HAB_CMD_SET, (bytes), (itm)),           \
        EXPAND_UINT32(value)

/*
 *    Set (MID location)
 */

#define SET_MID(bank, row, bit, fuses)                          \
    HDR(HAB_CMD_SET, SET_MID_BYTES, HAB_VAR_CFG_ITM_MID),       \
        (bank), (row), (bit), (fuses)

#define SET_MID_BYTES 8

/*
 *    Set (default ENG)
 */

#define SET_ENG(alg, eng, cfg)                                  \
    HDR(HAB_CMD_SET, SET_ENG_BYTES, HAB_VAR_CFG_ITM_ENG),       \
        0, (alg), (eng), (cfg)

#define SET_ENG_BYTES 8

/*
 *    Init (engine)
 */

#define INIT(eng)                                               \
    HDR(HAB_CMD_INIT, INIT_BYTES, (eng))

#define INIT_BYTES 4

/*
 *    Unlk (engine)
 */

#define UNLK(eng, ...)                          \
    UNLK_ ## eng(__VA_ARGS__)

#define UNLK_BYTES(eng, ...)                    \
    UNLK_BYTES_ ## eng(__VA_ARGS__)

#define UNLK_HDR(eng, ...)                                      \
    HDR(HAB_CMD_UNLK, UNLK_BYTES_ ## eng(__VA_ARGS__), eng)

#define UNLK_FLG(flg)  \
    0, 0, 0, (uint8_t)(flg)

#define UNLK_FLG_BYTES 4

#define UNLK_HAB_ENG_SRTC(dnc) UNLK_HDR(HAB_ENG_SRTC)
#define UNLK_BYTES_HAB_ENG_SRTC(dnc) HDR_BYTES

#define UNLK_HAB_ENG_SNVS(flg) UNLK_HDR(HAB_ENG_SNVS), UNLK_FLG(flg)
#define UNLK_BYTES_HAB_ENG_SNVS(flg) (HDR_BYTES + UNLK_FLG_BYTES)

#define UNLK_HAB_ENG_CAAM(flg) UNLK_HDR(HAB_ENG_CAAM), UNLK_FLG(flg)
#define UNLK_BYTES_HAB_ENG_CAAM(flg) (HDR_BYTES + UNLK_FLG_BYTES)

/* The next definition uses a GCC extension employing ## to swallow the
 * trailing comma in case the macro is called with only the fixed arguments
 * (i.e. flg here).  This extension appears to work in the GNU compatible mode
 * of RVDS and GHS compilers.
 */
#define UNLK_HAB_ENG_OCOTP(flg, ...)                            \
    UNLK_HDR(HAB_ENG_OCOTP, flg), UNLK_FLG(flg), ## __VA_ARGS__

#define UNLK_BYTES_HAB_ENG_OCOTP(flg, ...)              \
    (HDR_BYTES + UNLK_FLG_BYTES                         \
     + ( ((flg) & (HAB_OCOTP_UNLOCK_FIELD_RETURN        \
                   |HAB_OCOTP_UNLOCK_JTAG               \
                   |HAB_OCOTP_UNLOCK_SCS))              \
         ? STUB_FAB_UID_BYTES                           \
         : 0 ))

#if 0
/* Note: no comma after HDR().  Supplied by _VAL macro if needed */
#define UNLK(eng, val)                               \
    HDR(HAB_CMD_UNLK, UNLK_BYTES_ ## eng, (eng))     \
    UNLK_VAL_ ## eng(val)

#define UNLK_BYTES(eng)                         \
    UNLK_BYTES_ ## eng

#define UNLK_BYTES_HAB_ENG_SRTC HDR_BYTES
#define UNLK_VAL_HAB_ENG_SRTC(val)      /* no val field */
#define UNLK_BYTES_HAB_ENG_SNVS (HDR_BYTES + 4)
#define UNLK_VAL_HAB_ENG_SNVS(val) ,0,0,0,((val)&0xff)
#define UNLK_BYTES_HAB_ENG_CAAM (HDR_BYTES + 4)
#define UNLK_VAL_HAB_ENG_CAAM(val) ,0,0,0,((val)&0xff)
#endif

/*
 *    NOP
 */

#define NOP()                                                           \
    HDR(HAB_CMD_NOP, NOP_BYTES, 0xae) /* third param is ignored */

#define NOP_BYTES 4

/*
 *    Install Key (generic - used internally only)
 */

#define INS_KEY(bytes, flg, pcl, alg, src, tgt, crt)    \
    HDR(HAB_CMD_INS_KEY, (bytes), (flg)),               \
        (pcl), (alg), (src), (tgt),                     \
        EXPAND_UINT32(crt)

#define INS_KEY_BASE_BYTES 12

/*
 *    Install Key (SRK)
 */

#define INS_SRK(flg, alg, src, crt)                     \
    INS_KEY(INS_SRK_BYTES, (flg),                       \
            HAB_PCL_SRK, (alg), (src), HAB_IDX_SRK,     \
            (crt))

#define INS_SRK_BYTES INS_KEY_BASE_BYTES

/*
 *    Install Key (CSFK)
 */

#define INS_CSFK(flg, pcl, crt)                                 \
    INS_KEY(INS_CSFK_BYTES, (flg) | HAB_CMD_INS_KEY_CSF,        \
            (pcl), HAB_ALG_ANY, HAB_IDX_SRK, HAB_IDX_CSFK,      \
            (crt))

#define INS_CSFK_BYTES INS_KEY_BASE_BYTES

/*
 *    Install Key (IMGK - no hash)
 */

#define INS_IMGK(flg, pcl, src, tgt, crt)       \
    INS_KEY(INS_IMGK_BYTES, (flg),              \
            (pcl), HAB_ALG_ANY, (src), (tgt),   \
            (crt))

#define INS_IMGK_BYTES INS_KEY_BASE_BYTES


/*
 *    Install Key (IMGK - with hash). Must be followed by the crt_hsh contents
 *    (e.g. using #include).  The length field depends on using one of the
 *    standard HAB algorithm names, with no adornments like casts or
 *    parentheses.  Note that the length macro cannot be used here: the ##
 *    must appear in the body of this macro to prevent the alg parameter from
 *    being expanded first.
 */

#define INS_IMGK_HASH(flg, pcl, alg, src, tgt, crt)                     \
    INS_KEY(INS_KEY_BASE_BYTES + BYTES_ ## alg, (flg) | HAB_CMD_INS_KEY_HSH, \
            (pcl), (alg), (src), (tgt),                                 \
            (crt))

/*
 * Same as above but the hash length is fixed to the length of SHA1,
 * but the algorithm remains unchanged.
 */
#define INS_IMGK_INV_HASH(flg, pcl, alg, src, tgt, crt)                 \
    INS_KEY(INS_IMGK_HASH_BYTES(HAB_ALG_SHA1), (flg) | HAB_CMD_INS_KEY_HSH, \
            (pcl), (alg), (src), (tgt),                                 \
            (crt))


#define INS_IMGK_HASH_BYTES(alg)                \
    (INS_KEY_BASE_BYTES + BYTES_ ## alg)

#define BYTES_HAB_ALG_SHA1   20
#define BYTES_HAB_ALG_SHA256 32
#define BYTES_HAB_ALG_SHA512 64
/* dummy value for invalid hash alg - same as default hash algorithm */
#define DEFAULT_HASH_ALG_BYTES BYTES_HAB_ALG_SHA256
#define BYTES_HAB_ALG_PKCS1  DEFAULT_HASH_ALG_BYTES

/*
 *    Authenticate Data (generic - used internally only)
 */

#define AUT_DAT(bytes, flg, key, pcl, eng, cfg, sig_start)      \
    HDR(HAB_CMD_AUT_DAT, (bytes), (flg)),                       \
        (key), (pcl), (eng), (cfg),                             \
        EXPAND_UINT32(sig_start)

#define AUT_DAT_BASE_BYTES 12

/*
 *    Authenticate Data (CSF)
 */

#define AUT_CSF(flg, pcl, eng, cfg, sig_start)  \
    AUT_DAT(AUT_CSF_BYTES, (flg),               \
            HAB_IDX_CSFK, (pcl), (eng), (cfg),  \
            (sig_start))

#define AUT_CSF_BYTES AUT_DAT_BASE_BYTES

/*
 *    Authenticate Data (Image)
 */

#define AUT_IMG(blocks, flg, key, pcl, eng, cfg, sig_start)     \
    AUT_DAT(AUT_IMG_BYTES(blocks), (flg),                       \
            (key), (pcl), (eng), (cfg),                         \
            (sig_start))

#define AUT_IMG_BYTES(blocks)                   \
    (AUT_DAT_BASE_BYTES + 8*(blocks))

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
#endif

