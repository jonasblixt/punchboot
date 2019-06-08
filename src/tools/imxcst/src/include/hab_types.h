#ifndef HAB_TYPES_H
#define HAB_TYPES_H
/*===========================================================================*/
/**
    @file    hab_types.h

    @brief Constants, macros and types common to both external interfaces and
    internal modules.

@verbatim
=============================================================================

               Freescale Semiconductor
          (c) Freescale Semiconductor, Inc. 2007-2016.
          Copyright 2017-2019 NXP

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

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
                            INCLUDE FILES
=============================================================================*/

#include <stdint.h>             /* for integer types */
#include <stdbool.h>            /* for bool type */
#include <stddef.h>             /* for NULL and offsetof() */

/*===========================================================================
                              CONSTANTS
=============================================================================*/

/** @addtogroup struct
 *  @{ 
 */

/** @name External data structure tags
 * @anchor dat_tag
 *
 * Tag values 0x00 .. 0xef are reserved for HAB.  Values 0xf0 .. 0xff
 * are available for custom use.
 */
/*@{*/
#define HAB_TAG_IVT0 0xd1         /**< Image Vector Table V0 */
#define HAB_TAG_IVT  HAB_TAG_IVT0 /**< Alias for Image Vector Table V0 */
#define HAB_TAG_DCD  0xd2         /**< Device Configuration Data */
#define HAB_TAG_CSF  0xd4         /**< Command Sequence File */
#define HAB_TAG_CRT  0xd7         /**< Certificate */
#define HAB_TAG_SIG  0xd8         /**< Signature */
#define HAB_TAG_EVT  0xdb         /**< Event */
#define HAB_TAG_RVT  0xdd         /**< ROM Vector Table */
#define HAB_TAG_IVT1 0xde         /**< Image Vector Table V1 */
#define HAB_TAG_WRP  0x81         /**< Wrapped Key */
#define HAB_TAG_MAC  0xac         /**< Message Authentication Code */
/* Values 00 ... 7e reserved for internal use.  Values b0 ... cf reserved for
 * CSF commands.  Values e0 ... ef reserved for key types.
 *
 * Available values: 82, 84, 87, 88, 8b, 8d, 8e, 90, 93, 95, 96, 99, 9a,
 * 9c, 9f, a0, a3, a5, a6, a9, aa, af
 *
 * Custom values: f0, f3, f5, f6, f9, fa, fc, ff
 */
/*@}*/

/** @name HAB version */
/*@{*/
#define HAB_MAJOR_VERSION  4    /**< Major version of this HAB release */
#define HAB_MINOR_VERSION  4    /**< Minor version of this HAB release */
#define HAB_PATCH_VERSION  0    /**< Patch version of this HAB release */

#define HAB_VER_MAJ_WIDTH 4     /**< Major version field width  */
#define HAB_VER_MAJ_SHIFT 4     /**< Major version field offset  */
#define HAB_VER_MIN_WIDTH 4     /**< Minor version field width  */
#define HAB_VER_MIN_SHIFT 0     /**< Minor version field offset  */

#define HAB_FULL_VER_MAJ_WIDTH 8     /**< Major version field width  */
#define HAB_FULL_VER_MAJ_SHIFT 16    /**< Major version field offset  */
#define HAB_FULL_VER_MIN_WIDTH 8     /**< Minor version field width  */
#define HAB_FULL_VER_MIN_SHIFT 8     /**< Minor version field offset  */
#define HAB_FULL_VER_PAT_WIDTH 8     /**< Patch version field width  */
#define HAB_FULL_VER_PAT_SHIFT 0     /**< Patch version field offset  */

/** Full version of this HAB release @hideinitializer */
#define HAB_FULL_VERSION HAB_FULL_VER(HAB_MAJOR_VERSION, \
                                      HAB_MINOR_VERSION, \
                                      HAB_PATCH_VERSION)

/** Abbreviated version of this HAB release @hideinitializer */
#define HAB_VERSION HAB_VER(HAB_MAJOR_VERSION, HAB_MINOR_VERSION)

/** Base version for this HAB release @hideinitializer */
#define HAB_BASE_VERSION HAB_VER(HAB_MAJOR_VERSION, 0) 

/*@}*/

/*  @} struct */

/*---------------------------------------------------------------------------*/

/** @addtogroup cmd
 *  @{ 
 */

/** @name Command tags
 * @anchor cmd_tag
 *
 * Tag values 0xb0 .. 0xcf are reserved for HAB.  Values 0xf0 .. 0xff
 * are available for custom use.
 */
/*@{*/
#define HAB_CMD_SET       0xb1  /**< Set */
#define HAB_CMD_INS_KEY   0xbe  /**< Install Key */
#define HAB_CMD_AUT_DAT   0xca  /**< Authenticate Data */
#define HAB_CMD_WRT_DAT   0xcc  /**< Write Data */
#define HAB_CMD_CHK_DAT   0xcf  /**< Check Data */
#define HAB_CMD_NOP       0xc0  /**< No Operation */
#define HAB_CMD_INIT      0xb4  /**< Initialise */
#define HAB_CMD_UNLK      0xb2  /**< Unlock */
#ifdef HAB_FUTURE
#define HAB_CMD_RMV_KEY         /**< Remove Key */
#define HAB_CMD_INS_REF         /**< Install Reference Data */
#define HAB_CMD_INS_PLG         /**< Install Plugin */
#define HAB_CMD_RMV_PLG         /**< Remove Plugin */
#define HAB_CMD_CHK_VER         /**< Check SW Version */
#endif
/* Remaining values: b7, b8, bb, bd, c3, c5, c6, c9 */
/*@}*/

/*  @} cmd */


/** @name SRK set identifiers
 *
 * Values must be sequential as they are also used to create a mask
 */
/*@{*/
#define HAB_SRKSET_OEM 0
#define HAB_SRKSET_NXP 1
/*@}*/

/*---------------------------------------------------------------------------*/

/** @addtogroup pcl
 *  @{ 
 */

/** @name Protocol tags
 * @anchor pcl_tag 
 *
 * Tag values 0x00 .. 0xef are reserved for HAB.  Values 0xf0 .. 0xff are
 * available for custom use.
 */
/*@{*/
#define HAB_PCL_SRK      0x03   /**< SRK certificate format */
#define HAB_PCL_X509     0x09   /**< X.509v3 certificate format */
#define HAB_PCL_CMS      0xc5   /**< CMS/PKCS#7 signature format */
#define HAB_PCL_BLOB     0xbb   /**< SHW-specific wrapped key format */
#define HAB_PCL_AEAD     0xa3   /**< Proprietary AEAD MAC format */
#ifdef HAB_FUTURE
#define HAB_PCL_WTLS     0x05   /**< OMA WTLS certificate format */
#define HAB_PCL_FSL      0x0f   /**< FSL bound signature protocol */
#define HAB_PCL_HMAC     0x30   /**< NIST HMAC message authentication */
#define HAB_PCL_CBCMAC   0x33   /**< CBC-MAC message authentication */
#endif
/*@}*/

/* Available values: 06, 0a, 0c, 11, 12, 14, 17, 18, 1b, 1d, 1e, 21, 22, 24,
 * 27, 28, 2b, 2d, 2e, 35, 36, 39, 3a, 3c, 3f, 41, 42, 44, 47, 48, 4b, 4d, 4e,
 * 50, 53, 55, 56, 59, 5a, 5c, 5f, 60, 63, 65, 66, 69, 6a, 6c, 6f, 71, 72, 74,
 * 77, 78, 7b, 7d, 7e, 81, 82, 84, 87, 88, 8b, 8d, 8e, 90, 93, 95, 96, 99, 9a,
 * 9c, 9f, a0, a5, a6, a9, aa, ac, af, b1, b2, b4, b7, b8, bd, be, c0, c3, c6,
 * c9, ca, cc, cf, d1, d2, d4, d7, d8, db, dd, de, e1, e2, e4, e7, e8, eb, ed,
 * ee
 *
 * Custom values: f0, f3, f5, f6, f9, fa, fc, ff
 */

/*  @} pcl */

/*---------------------------------------------------------------------------*/

/** @addtogroup alg
 *  @{ 
 */

/** @name Algorithm types
 * @anchor alg_typ
 *
 * The most-significant nibble of an algorithm ID denotes the algorithm
 * type.  Algorithms of the same type share the same interface.
 *
 * Types 0x0 .. 0xc are reserved for HAB.  Types 0xd .. 0xf are available for
 * custom use.  Within each reserved type N in 0 .. c, tag values 0xN0 .. 0xNc
 * are reserved for HAB.  Values 0xNd .. 0xNf are available for custom use.
 */
/*@{*/
#define HAB_ALG_ANY      0x0    /**< Algorithm type ANY */
#define HAB_ALG_HASH     0x1    /**< Hash algorithm type */
#define HAB_ALG_SIG      0x2    /**< Signature algorithm type */
#define HAB_ALG_FF       0x3    /**< Finite field arithmetic */
#define HAB_ALG_EC       0x4    /**< Elliptic curve arithmetic */
#define HAB_ALG_CIPHER   0x5    /**< Cipher algorithm type */
#define HAB_ALG_MODE     0x6    /**< Cipher/hash modes */
#define HAB_ALG_WRAP     0x7    /**< Key wrap algorithm type */
/*@}*/

/** @name Algorithm type ANY
 *
 * Algorithms of type ANY have no common interface: the protocol must know
 * what to do.
 */
/*@{*/
#ifdef HAB_FUTURE
#define HAB_ALG_RANDOM          /**< Random number generation */
#endif
/* Available values: 03, 05, 06, 09, 0a, 0c, 0f
 */
/*@}*/

/** @name Hash algorithms */
/*@{*/
#define HAB_ALG_SHA1     0x11   /**< SHA-1 algorithm ID */
#define HAB_ALG_SHA256   0x17   /**< SHA-256 algorithm ID */
#define HAB_ALG_SHA384   0x18   /**< SHA-384 algorithm ID */
#define HAB_ALG_SHA512   0x1b   /**< SHA-512 algorithm ID */
/* Available values: 0x14, 0x12, 1d, 1e
 */
/*@}*/

/** @name Signature algorithms */
/*@{*/
#define HAB_ALG_PKCS1    0x21   /**< PKCS#1 RSA signature algorithm  */
#define HAB_ALG_ECDSA    0x27   /**< NIST ECDSA signature algorithm */
#ifdef HAB_FUTURE
#define HAB_ALG_DSA             /**< NIST DSA signature algorithm */
#endif
/* Available values: 22, 24, 28, 2b, 2d, 2e
 */
/*@}*/

/** @name Elliptic curve domain parameters */
/*@{ */
#define HAB_EC_P112       0x41  /**< WTLS/SECG secp112r1 curve */
#define HAB_EC_P256       0x4b  /**< NIST-approved P-256 curve */
#define HAB_EC_P384       0x4d  /**< NIST-approved P-384 curve */
#define HAB_EC_P521       0x4e  /**< NIST-approved P-521 curve */
/*@}*/

/** @name Cipher algorithms */
/*@{*/
#define HAB_ALG_AES      0x55   /**< AES algorithm ID */
/* Available values: 50, 53, 56, 59, 5a, 5c, 5f
 */
/*@}*/

/** @name Cipher or hash modes */
/*@{*/
#define HAB_MODE_CCM      0x66  /**< Counter with CBC-MAC */
#ifdef HAB_FUTURE
#define HAB_MODE_HMAC           /**< HMAC hash mode */
#endif
/* Available values: 60, 63, 65, 69, 6a, 6c, 6f
 */
/*@}*/

/** @name Key wrap algorithms */
/*@{*/
#define HAB_ALG_BLOB      0x71  /**< SHW-specific key wrap */
/* Available values: 72, 74, 77, 78, 7b, 7d, 7e
 */
/*@}*/

/* Available values: 81, 82, 84, 87, 88, 8b, 8d, 8e, 90, 93, 95, 96, 99, 9a,
 * 9c, 9f, a0, a3, a5, a6, a9, aa, ac, af, b1, b2, b4, b7, b8, bb, bd, be, c0,
 * c3, c5, c6, c9, ca, cc, cf, d1, d2, d4, d7, d8, db, dd, de, e1, e2, e4, e7,
 * e8, eb, ed, ee, f0, f3, f5, f6, f9, fa, fc, ff
 */

/*  @} alg */

/*---------------------------------------------------------------------------*/

/** @addtogroup eng
 *  @{ 
 */

/** @name Engine plugin tags 
 *  @anchor eng_tag
 *
 * Tag values 0x00 .. 0xef and 0xff are reserved for HAB.  Values 0xf0 .. 0xfe
 * are available for custom use.
 */
/*@{*/
#define HAB_ENG_ANY      0x00   /**< First compatible engine will be
                                 * selected automatically (no engine
                                 * configuration parameters are allowed).
                                 */
#define HAB_ENG_SCC      0x03   /**< Security controller */
#define HAB_ENG_RTIC     0x05   /**< Run-time integrity checker */
#define HAB_ENG_SAHARA   0x06   /**< Crypto accelerator */
#define HAB_ENG_CSU      0x0a   /**< Central Security Unit */
#define HAB_ENG_SRTC     0x0c   /**< Secure clock */
#ifdef HAB_FUTURE
#define HAB_ENG_RNG      0x09   /**< Standalone random number generator */
#define HAB_ENG_SJC      0x0f   /**< Secure JTAG controller */
#define HAB_ENG_WDOG     0x11   /**< Watchdog timer */
#define HAB_ENG_SRC      0x12   /**< System Reset Controller */
#define HAB_ENG_SPBA     0x14   /**< Shared Peripheral Bus Arbiter */
#define HAB_ENG_IIM      0x17   /**< Fuse controller */
#define HAB_ENG_IOMUX    0x18   /**< IO multiplexer */
#endif
#define HAB_ENG_DCP      0x1b   /**< Data Co-Processor */
#define HAB_ENG_CAAM     0x1d   /**< Cryptographic Acceleration and
                                     Assurance Module */
#define HAB_ENG_SNVS     0x1e   /**< Secure Non-Volatile Storage */
#define HAB_ENG_OCOTP    0x21   /**< Fuse controller */
/** @cond rom */
#define HAB_ENG_DTCP     0x22   /**< DTCP co-processor */
#define HAB_ENG_HDMI     0x27   /**< HDMI firmware processing */
#define HAB_ENG_ROM      0x36   /**< Protected ROM area */
#define HAB_ENG_HDCP     0x24   /**< HDCP co-processor */
#define HAB_ENG_RTL      0x77   /**< @rom RTL simulation engine */
/** @endcond */
#define HAB_ENG_SW       0xff   /**< Software engine */
/* Available values: 28, 2b, 2d, 2e, 30, 33, 35,
 * 39, 3a, 3c, 3f, 41, 42, 44, 47, 48, 4b, 4d, 4e, 50, 53, 55, 56, 59, 5a,
 * 5c, 5f, 60, 63, 65, 66, 69, 6a, 6c, 6f, 71, 72, 74, 78, 7b, 7d, 7e, 81,
 * 82, 84, 87, 88, 8b, 8d, 8e, 90, 93, 95, 96, 99, 9a, 9c, 9f, a0, a3, a5, a6,
 * a9, aa, ac, af, b1, b2, b4, b7, b8, bb, bd, be, c0, c3, c5, c6, c9, ca, cc,
 * cf, d1, d2, d4, d7, d8, db, dd, de, e1, e2, e4, e7, e8, eb, ed, ee
 *
 * Custom values: f0, f3, f5, f6, f9, fa, fc
 */
/*@}*/

/*  @} eng */

/*---------------------------------------------------------------------------*/

/** @addtogroup sah
 *  @{ 
 */

/** Maximum data blocks in a single hash  */
#define HAB_SAHARA_BLOCK_MAX 12

/** @cond rom */
/** @rom DMA storage requirement
 *
 * This figure is derived in several parts:
 * - each hash operation needs a 6-word descriptor structure
 * - each data block needs a 3-word link structure
 * - the result needs a 3-word link structure
 * - at least 40 bytes are required for SHA-256 result and memory manager
 * overhead: 64 bytes allows some small overhead.
 */
#define HAB_SAHARA_DMA_MIN_BYTES (24 + HAB_SAHARA_BLOCK_MAX * 12 + 12 + 64)
/** @endcond */

/*  @} sah */

/*---------------------------------------------------------------------------*/

/** @addtogroup dcp
 *  @{ 
 */

/** Maximum data blocks in a single hash */
#define HAB_DCP_BLOCK_MAX 6

/** @cond rom */
/** @rom DMA storage requirement
 *
 * This figure is derived in two parts:
 * - each data block needs an 8-word work packet (descriptor)
 * - at least 40 bytes are required for SHA-256 result and memory manager
 * overhead: 64 bytes allows some small overhead.
 */
#define HAB_DCP_DMA_MIN_BYTES (64 + HAB_DCP_BLOCK_MAX * 32)
/** @endcond */

/*  @} dcp */

/*---------------------------------------------------------------------------*/

/** @addtogroup rtic
 *  @{ 
 */

/** Maximum data blocks in a single hash */
#define HAB_RTIC_BLOCK_MAX 2

/*  @} rtic */

/*---------------------------------------------------------------------------*/

/** @addtogroup scc
 *  @{ 
 */

/** @cond rom */
/** @rom DMA storage requirement
 *
 * This figure is derived in several stages, and assumes plaintext and
 * ciphertext buffers are both allocated in the DMA region :
 * - 4 blocks of plaintext required
 * - 4 blocks of ciphertext required
 * - each block is 16 bytes long
 * - the plaintext address must be block-aligned (up to 15 bytes overhead)
 * - the ciphertext address must be block-aligned (up to 3 bytes overhead)
 * - at least 8 bytes of memory manager overhead: allow 32 for comfort
 */
#define HAB_SCC_DMA_MIN_BYTES ( (4+4)*16 + 15 + 3 + 32)
/** @endcond */

/*  @} scc */

/*---------------------------------------------------------------------------*/

/** @addtogroup caam
 *  @{ 
 */

/** Maximum data blocks in an @ref cmd_aut_dat command */
#define HAB_CAAM_BLOCK_MAX 8

/** @cond rom */
/** @rom Hash DMA storage requirement
 *
 * This figure is derived in several parts:
 * - each hash operation needs
 *   - a 7-word descriptor, and
 *   - a 64-byte result buffer (for SHA-512),
 *   - giving a base requirement of (7*4 + 64) = 92 bytes
 * - each data block needs a 4-word link structure
 * - memory manager overhead is at least 8 bytes
 */
#define HAB_CAAM_HSH_DMA_MIN_BYTES (92 + HAB_CAAM_BLOCK_MAX * 16 + 8)

/** @rom AEAD DMA storage requirement
 *
 * This figure is derived in several parts:
 * - each AEAD operation needs
 *   - a 16-word descriptor, 
 *   - a 32-byte initial context value (B0 and CTR0), and
 *   - a 16-byte MAC value,
 *   - giving a base requirement of (16*4 + 32 + 16) = 112 bytes
 * - each data block needs a 4-word link structure
 * - memory manager overhead is at least 8 bytes: 16 bytes allows flexibility
 */
#define HAB_CAAM_CCM_DMA_MIN_BYTES (112 + HAB_CAAM_BLOCK_MAX * 16 + 16)

/** @rom ECDSA DMA storage requirement
 *
 * This figure is derived in several parts:
 * - each ECDSA operation needs
 *   - a 12-word descriptor,
 *   - six 66-byte buffers for curve parameters (P-521)
 *   - two 66-byte buffers for public key parameters (P-521)
 *   - two 66-byte buffers for signature parameters (P-521)
 *   - one 66-byte buffers for message representative (P-521)
 *   - two 66-byte buffers for working space (P-521)
 *   - two words to store the parameter sizes
 *   - giving a base requirement of (12*4 + 13*66 + 2*4) = 914 bytes
 * - memory manager overhead is 4 bytes (one header word)
 */
#define HAB_CAAM_ECDSA_DMA_MIN_BYTES (914 + 4)

/** @rom RNG DMA storage requirement
 *
 * This figure is derived in several parts:
 * - each DRNG test operation allocates a DMA area with
 *   - a 1-word header, and
 *   - a 3-word job ring area, and
 *   - a 54-word descriptor,
 *   - requiring a total 58*4 = 232 bytes
 * - each DRNG test operation also allocates a DMA area with
 *   - a 1-word header, and
 *   - a 32-byte result buffer
 *   - requiring a total 4 + 32 = 36 bytes
 */
#define HAB_CAAM_RNG_DMA_MIN_BYTES (232 + 32)
/** @endcond */

/*  @} caam */

/*---------------------------------------------------------------------------*/

/** @addtogroup key
 *  @{ 
 */

/** @name Key types
 * @anchor key_types
 *
 * Tag values 0xe0 .. 0xef are reserved for HAB.  Values 0xf0 .. 0xff
 * are available for custom use.
 */
/*@{*/
#define HAB_KEY_PUBLIC 0xe1     /**< Public key type: data present */
#define HAB_KEY_SECRET 0xe2     /**< Secret key type: data present */
#define HAB_KEY_MASTER 0xed     /**< Master KEK type */
#define HAB_KEY_HASH   0xee     /**< Any key type: hash only */
/* Available values: e4, e7, e8, eb
 *
 * Custom values: f0, f3, f5, f6, f9, fa, fc, ff
 */
/*@}*/

/** @name Public key store indices */
/*@{*/
#define HAB_IDX_SRK   0         /**< Super-Root Key, set 0 index */
#define HAB_IDX_SRK0  0         /**< Super-Root Key, set 0 index */
#define HAB_IDX_CSFK  1         /**< CSF key 0 index */
#define HAB_IDX_CSFK0 1         /**< CSF key 0 index */
#define HAB_IDX_SRK1  5         /**< Super-Root Key, set 1 index */
#define HAB_IDX_CSFK1 6         /**< CSF key 1 index  */
/*@}*/

/** @name Key Counts */
/*@{*/
#define HAB_SRK_MIN 1           /**< Minimum Super-Root Key count */
#define HAB_SRK_MAX 4           /**< Maximum Super-Root Key count */
#define HAB_KEY_PUBLIC_MAX 8    /**< Maximum installed public key count
                                 *   (incl Super-Root Key)
                                 */
#define HAB_KEY_SECRET_MAX 6    /**< Maximum installed secret key count
                                 *   (excl Master KEKs)
                                 */
/*@}*/

/*  @} key */

/*---------------------------------------------------------------------------*/

/** @addtogroup key_ecdsa
 *  @{
 */

/** @name ECDSA Key Offsets */
/*@{*/
#define HAB_KEY_ECDSA_CURVE_OFFSET 0 /**< Offset of @a curve field in bytes */
#define HAB_KEY_ECDSA_RED_BITS_OFFSET 2 /**< Offset of @a red_bits field in
                                         *   bytes */
#define HAB_KEY_ECDSA_QX_OFFSET 4 /**< Offset of @a Qx field in bytes */
/*@}*/

/*  @} key_ecdsa */

/*---------------------------------------------------------------------------*/

/** @addtogroup key_pkcs1
 *  @{ 
 */

/** @name Bitfield definitions */
/*@{*/
#define HAB_KEY_PKCS1_FLG_WIDTH 8 /**< Width of @a flg field */
#define HAB_KEY_PKCS1_FLG_SHIFT 0 /**< Offset of @a flg field */
#define HAB_KEY_PKCS1_MODBYTES_WIDTH 16 /**< Width of mod_bytes field */
#define HAB_KEY_PKCS1_MODBYTES_SHIFT 16 /**< Offset of mod_bytes field */
#define HAB_KEY_PKCS1_EXPBYTES_WIDTH 16 /**< Width of exp_bytes field */
#define HAB_KEY_PKCS1_EXPBYTES_SHIFT 0 /**< Offset of exp_bytes field */
/*@}*/

/** @name Binding flag bitfield definitions */
/*@}*/
#define HAB_KEY_BND_FLG_WIDTH 5 /**< Width of binding flags */
#define HAB_KEY_BND_FLG_SHIFT 2 /**< Offset of binding flags */
/*@}*/

/*  @} key_pkcs1 */

/*---------------------------------------------------------------------------*/

/** @addtogroup cmd_wrt_dat
 *  @{
 */

/** @name Parameter bitfield definitions.
 *
 *  Apply to both @ref cmd_wrt_dat and @ref cmd_chk_dat commands. */
/*@{*/
#define HAB_CMD_WRT_DAT_FLAGS_WIDTH 5   /**< @a flags field width */
#define HAB_CMD_WRT_DAT_FLAGS_SHIFT 3   /**< @a flags field offset */
#define HAB_CMD_WRT_DAT_BYTES_WIDTH 3   /**< @a bytes field width */
#define HAB_CMD_WRT_DAT_BYTES_SHIFT 0   /**< @a bytes field offset */
/*@}*/

/*  @} cmd_wrt_dat */

/*---------------------------------------------------------------------------*/

/** @addtogroup bnd_obj
 *  @{
 */

/** @name Binding object IDs 
 * @anchor bnd_ids
 *
 * The ASN.1 object identifiers used to identify HAB binding attributes are
 * defined in the following arc:
 *
@verbatim
      id-fsl  OBJECT IDENTIFIER ::= {
           joint-iso-itu-t(2) country(16) us(840) organization(1) fsl(123456) }
  
      id-habBnd OBJECT IDENTIFIER  ::=  {
           id-fsl hab(32) binding-objects(16) }

      id-habBnd-dat OBJECT IDENTIFIER  ::=  {
           id-habBnd dat(1) }

      id-habBnd-cfg OBJECT IDENTIFIER  ::=  {
           id-habBnd cfg(3) }

      id-habBnd-fid OBJECT IDENTIFIER  ::=  {
           id-habBnd fid(5) }

      id-habBnd-mid OBJECT IDENTIFIER  ::=  {
           id-habBnd mid(6) }

      id-habBnd-cid OBJECT IDENTIFIER  ::=  {
           id-habBnd cid(9) }
@endverbatim
 *
 * The ASN.1 object identifiers used to identify HAB binding attributes are
 * single component extensions of id-habBnd using a component value less than
 * 128 (so that the component can be DER-encoded in a single byte).
 *
 * The DER encoding of an object identifier in this arc is the concatenation
 * of the DER prefix with the single byte identifier for the required binding
 * object. Binding object attribute values are encoded as an ASN.1 SET with
 * a single OCTET STRING member.
 */
/*@{*/

/** DER prefix
 *
 * @todo update description and encoding of binding object identifiers with
 * real fsl value instead of fsl(123456) encoded as 0x87, 0xc4, 0x40, and
 * confirm chosen values for hab(32) and binding-objects(16).
 */
#define HAB_BND_DER_PREFIX \
    {0x06, 0x0a, 0x60, 0x86, 0x48, 0x01, 0x87, 0xc4, 0x40, 0x20, 0x10}
#define HAB_BND_DAT 0x01        /**< Data type (mandatory) */
#define HAB_BND_CFG 0x03        /**< Security configuration */
#define HAB_BND_FID 0x05        /**< Fabrication UID */
#define HAB_BND_MID 0x06        /**< Manufacturing ID */
#define HAB_BND_CID 0x09        /**< Caller ID */
/* Available values: 0a, 0c, 0f, 11, 12, 14, 17, 18, 1b, 1d, 1e, 21, 22, 24,
 * 27, 28, 2b, 2d, 2e, 30, 33, 35, 36, 39, 3a, 3c, 3f, 41, 42, 44, 47, 48, 4b,
 * 4d, 4e, 50, 53, 55, 56, 59, 5a, 5c, 5f, 60, 63, 65, 66, 69, 6a, 6c, 6f, 71,
 * 72, 74, 77, 78, 7b, 7d, 7e
 */
/*@}*/


/** @name Caller IDs
 *
 * Only the ROM caller ID is defined, but other caller IDs may be defined by
 * later boot stages.
 */
/*@{*/
#define HAB_CID_ROM 0   /**< ROM Caller ID */
/*@}*/

/*  @} bnd_obj */

#ifdef HAB_FUTURE
/** @addtogroup sig_fsl
 *  @{
 */

#define HAB_BND_DAT_BYTES 512   /**< Maximum binding data size */

/*  @} sig_fsl */
#endif

/*===========================================================================
                                MACROS
=============================================================================*/


/** @cond rom */

/** @addtogroup hal
 *  @{
 */

/** @name Miscellaneous macros */
/*@{*/

/** @rom Count array entries
 *@hideinitializer
 *
 * @param[in] a fixed size array
 *
 * @return Array entries
 */
#define HAB_ENTRIES_IN(a) \
    (sizeof(a) / sizeof((a)[0]))


/** @rom Cast 32 bit value to void pointer
 *@hideinitializer
 *
 * @param[in] Value to cast
 *
 * @return void *
 */
#define HAB_PTR32(v) ((void *)((uintptr_t)(v)))

/** @rom Cast 64 bit value to void pointer
 *@hideinitializer
 *
 * @param[in] Value to cast
 *
 * @return void *
 */
#if (__SIZEOF_POINTER__ == 4)
#  define HAB_PTR64(v) ((void *)((uint32_t)(v)))
#elif (__SIZEOF_POINTER__ == 8)
#  define HAB_PTR64(v) ((void *)((uint64_t)(v)))
#else
#  error Need to know size of pointer
#endif

/*@}*/

/*  @} hal */

/** @endcond */

/*===========================================================================
                                ENUMS
=============================================================================*/

/** Supported widths of data commands.
 * @ingroup cmd_wrt_dat
 */
typedef enum hab_data_width
{
    HAB_DATA_WIDTH_BYTE = 1,    /**< 8-bit value */
    HAB_DATA_WIDTH_HALF = 2,    /**< 16-bit value */
    HAB_DATA_WIDTH_WORD = 4     /**< 32-bit value */
} hab_data_width_t;
    

/** Flags for Write Data commands.
 * @ingroup cmd_wrt_dat
 */
typedef enum hab_cmd_wrt_dat_flg
{
    HAB_CMD_WRT_DAT_MSK = 1,    /**< Mask/value flag: if set, only specific
                                 * bits may be overwritten at target address
                                 * (otherwise all bits may be overwritten)
                                 */
    HAB_CMD_WRT_DAT_SET = 2     /**< Set/clear flag: if #HAB_CMD_WRT_DAT_MSK
                                 * set, bits at the target address overwritten
                                 * with this flag (otherwise it is ignored)
                                 */
} hab_cmd_wrt_dat_flg_t;

/** Flags for Check Data commands.
 * @ingroup cmd_chk_dat
 */
typedef enum hab_cmd_chk_dat_flg
{
    HAB_CMD_CHK_DAT_SET = 2,    /**< Set/clear flag: bits set in mask must
                                 * match this flag
                                 */
    HAB_CMD_CHK_DAT_ANY = 4     /**< Any/all flag: if clear, all bits set in
                                 * mask must match (otherwise any bit
                                 * suffices)
                                 */
} hab_cmd_chk_dat_flg_t;

/** Flags for Authenticate Data commands.
 * @ingroup cmd_aut_dat
 */
typedef enum hab_cmd_aut_dat_flg
{
    HAB_CMD_AUT_DAT_CLR = 0,    /**< No flags set */
    HAB_CMD_AUT_DAT_ABS = 1     /**< Absolute signature address */
} hab_cmd_aut_dat_flg_t;

/** Flags for Install Key commands.
 * @ingroup cmd_ins_key
 */
typedef enum hab_cmd_ins_key_flg
{
    HAB_CMD_INS_KEY_CLR = 0,    /**< No flags set */
    HAB_CMD_INS_KEY_ABS = 1,    /**< Absolute certificate address */
    HAB_CMD_INS_KEY_CSF = 2,    /**< Install CSF key */
    HAB_CMD_INS_KEY_DAT = 4,    /**< Key binds to Data Type */
    HAB_CMD_INS_KEY_CFG = 8,    /**< Key binds to Configuration */
    HAB_CMD_INS_KEY_FID = 16,   /**< Key binds to Fabrication UID */
    HAB_CMD_INS_KEY_MID = 32,   /**< Key binds to Manufacturing ID */
    HAB_CMD_INS_KEY_CID = 64,   /**< Key binds to Caller ID */
    HAB_CMD_INS_KEY_HSH = 128   /**< Certificate hash present */
} hab_cmd_ins_key_flg_t;

/** Key flags.
 * @ingroup key_pkcs1
 *
 * @ifrom
 *
 * The binding flags given here align with those in #hab_cmd_ins_key_flg
 *
 * @endrom
 *
 */
typedef enum hab_key_flg
{
    /* Two more flag values available */
    HAB_KEY_FLG_DAT = 4,    /**< Key binds to Data Type */
    HAB_KEY_FLG_CFG = 8,    /**< Key binds to Configuration */
    HAB_KEY_FLG_FID = 16,   /**< Key binds to Fabrication UID */
    HAB_KEY_FLG_MID = 32,   /**< Key binds to Manufacturing ID */
    HAB_KEY_FLG_CID = 64,   /**< Key binds to Caller ID */
    HAB_KEY_FLG_CA  = 128   /**< CA key */
} hab_key_flg_t;

/** Secret key flags.
 * @ingroup crt_blob
 */
typedef enum hab_key_secret_flg
{
    /* Seven more flag values available */
    HAB_KEY_FLG_KEK  = 128   /**< KEK */
} hab_key_secret_flg_t;

/** Binding data types
 * @ingroup bnd_obj
 */
typedef enum hab_dat {
    HAB_DAT_CSF = 0x0f,         /**< CSF signature */
    HAB_DAT_IMG = 0x33,         /**< Image signature */
#ifdef HAB_FUTURE
    HAB_DAT_PLG = 0x3c,         /**< Plugin signature */
#endif
    HAB_DAT_MAX
} hab_dat_t;

/* Available values: 55, 5a, 66, 69, 96, 99, a5, aa, c3, cc, f0, ff
 */

/** Target check types
 * @ingroup chk_tgt
 */
typedef enum hab_target {
    HAB_TGT_MEMORY = 0x0f,      /**< Check memory white list */
    HAB_TGT_PERIPHERAL = 0xf0,  /**< Check peripheral white list */
    HAB_TGT_ANY = 0x55,         /**< Check memory & peripheral white list */
    HAB_TGT_MAX
} hab_target_t;

/** Security configuration types
 * @ingroup status
 */
typedef enum hab_config {
/** @cond rom */
    HAB_CFG_FAB = 0x00,         /**< @rom Un-programmed IC */
/** @endcond */
    HAB_CFG_RETURN = 0x33,      /**< Field Return IC */
    HAB_CFG_OPEN = 0xf0,        /**< Non-secure IC */
    HAB_CFG_CLOSED = 0xcc       /**< Secure IC */
} hab_config_t;
/* Available values: 0f, 3c, 55, 5a, 66, 69, 96, 99, a5, aa, ff
 */

/** Security state types
 * @ingroup status
 */
typedef enum hab_state {
    HAB_STATE_INITIAL = 0x33,   /**< Initialising state (transitory) */
    HAB_STATE_CHECK = 0x55,     /**< Check state (non-secure) */
    HAB_STATE_NONSECURE = 0x66, /**< Non-secure state */
    HAB_STATE_TRUSTED = 0x99,   /**< Trusted state */
    HAB_STATE_SECURE = 0xaa,    /**< Secure state */
    HAB_STATE_FAIL_SOFT = 0xcc, /**< Soft fail state */
    HAB_STATE_FAIL_HARD = 0xff, /**< Hard fail state (terminal) */
    HAB_STATE_NONE = 0xf0,      /**< No security state machine */
    HAB_STATE_MAX
} hab_state_t;
/* Available values: 00, 0f, 3c, 5a, 69, 96, a5, c3
 */

/** HAB status types
 * @ingroup status
 */
typedef enum hab_status {
    HAB_STS_ANY = 0x00,         /**< Match any status in
                                 * hab_rvt.report_event()
                                 */
    HAB_FAILURE = 0x33,         /**< Operation failed */
    HAB_WARNING = 0x69,         /**< Operation completed with warning */
    HAB_SUCCESS = 0xf0,         /**< Operation completed successfully */
    HAB_STS_MAX
} hab_status_t;

/** Failure or warning reasons
 * @ingroup evt
 *
 * Values 0x80 ... 0xff are reserved for internal use.
 */
typedef enum hab_reason {
    HAB_RSN_ANY = 0x00,         /**< Match any reason in
                                 * hab_rvt.report_event()
                                 */
    HAB_ENG_FAIL = 0x30,        /**< Engine failure. */
    HAB_INV_ADDRESS = 0x22,     /**< Invalid address: access denied. */
    HAB_INV_ASSERTION = 0x0c,   /**< Invalid assertion. */
    HAB_INV_CALL = 0x28,        /**< Function called out of sequence. */
    HAB_INV_CERTIFICATE = 0x21, /**< Invalid certificate. */
    HAB_INV_COMMAND = 0x06,     /**< Invalid command: command malformed. */
    HAB_INV_CSF = 0x11,         /**< Invalid @ref csf. */
    HAB_INV_DCD = 0x27,         /**< Invalid @ref dcd. */
    HAB_INV_INDEX = 0x0f,       /**< Invalid index: access denied. */
    HAB_INV_IVT = 0x05,         /**< Invalid @ref ivt. */
    HAB_INV_KEY = 0x1d,         /**< Invalid key. */
    HAB_INV_RETURN = 0x1e,      /**< Failed callback function. */
    HAB_INV_SIGNATURE = 0x18,   /**< Invalid signature. */
    HAB_INV_SIZE = 0x17,        /**< Invalid data size. */
    HAB_INV_BLOB = 0x3a,        /**< Invalid Blob */
    HAB_INV_MAC = 0x3c,         /**< Invalid MAC */
    HAB_MEM_FAIL = 0x2e,        /**< Memory failure. */
    HAB_OVR_COUNT = 0x2b,       /**< Expired poll count. */
    HAB_OVR_STORAGE = 0x2d,     /**< Exhausted storage region. */
    HAB_UNS_ALGORITHM = 0x12,   /**< Unsupported algorithm. */
    HAB_UNS_COMMAND = 0x03,     /**< Unsupported command. */
    HAB_UNS_ENGINE = 0x0a,      /**< Unsupported engine. */
    HAB_UNS_ITEM = 0x24,        /**< Unsupported configuration item. */
    HAB_UNS_KEY = 0x1b,         /**< Unsupported key type or parameters. */
    HAB_UNS_PROTOCOL = 0x14,    /**< Unsupported protocol. */
    HAB_UNS_STATE = 0x09,       /**< Unsuitable state. */
    HAB_RSN_MAX
} hab_reason_t;
/* Available values: 33, 35, 36, 39, 3a, 3c, 3f, 41, 42, 44,
 * 47, 48, 4b, 4d, 4e, 50, 53, 55, 56, 59, 5a, 5c, 5f, 60, 63, 65, 66, 69, 6a,
 * 6c, 6f, 71, 72, 74, 77, 78, 7b, 7d, 7e
 */

/** Audit logging contexts.
 * @ingroup evt
 *
 * This list is sorted in order of increasing priority: where two contexts
 * might apply, the latter one is used.
 *
 * Values 0x40 .. 0x5f are reserved for internal use.
 */
typedef enum hab_context {
    HAB_CTX_ANY = 0x00,         /**< Match any context in
                                 * hab_rvt.report_event()
                                 */
/** @cond rom */
    HAB_CTX_FAB = 0xff,         /**< @rom Event logged in hab_fab_test() */
/** @endcond */
    HAB_CTX_ENTRY = 0xe1,       /**< Event logged in hab_rvt.entry() */
    HAB_CTX_TARGET = 0x33,      /**< Event logged in hab_rvt.check_target() */
    HAB_CTX_AUTHENTICATE = 0x0a, /**< Event logged in
                                  *   hab_rvt.authenticate_image() 
                                  */
    HAB_CTX_DCD = 0xdd,         /**< Event logged in hab_rvt.run_dcd() */
    HAB_CTX_CSF = 0xcf,         /**< Event logged in hab_rvt.run_csf() */
    HAB_CTX_COMMAND = 0xc0,     /**< Event logged executing @ref csf or @ref
                                 *   dcd command
                                 */
    HAB_CTX_AUT_DAT = 0xdb,     /**< Authenticated data block */
    HAB_CTX_ASSERT = 0xa0,      /**< Event logged in hab_rvt.assert() */
    HAB_CTX_EXIT = 0xee,        /**< Event logged in hab_rvt.exit() */
    HAB_CTX_MAX
} hab_context_t;

/** Assertion types.
 * @ingroup assert
 */
typedef enum hab_assertion {
    HAB_ASSERT_BLOCK = 0, /**< Assert that a memory block was authenticated */
    HAB_ASSERT_MAX
} hab_assertion_t;

/** RTIC configuration flags 
 * @ingroup rtic
 */
typedef enum hab_rtic_config {
    HAB_RTIC_IN_SWAP8 = 0x01,   /**< Set BYTE SWAP bit (reverse bytes within
                                 *   word on input to RTIC) */
    HAB_RTIC_IN_SWAP16 = 0x02,  /**< Set HALF WORD SWAP bit (reverse
                                 *   half-words within word on input to
                                 *   RTIC)  */
    HAB_RTIC_OUT_SWAP8 = 0x08,  /**< Set HASH RESULT BYTE SWAP bit (reverse
                                 *   bytes within word on output from RTIC) */
    HAB_RTIC_KEEP = 0x80        /**< Retain reference hash value for later
                                 *   monitoring */
} hab_rtic_config_t;

/** SAHARA configuration flags 
 * @ingroup sah
 */
typedef enum hab_sahara_config {
    HAB_SAHARA_IN_SWAP8 = 0x01,   /**< Set MESS BYTE SWAP bit (reverse message
                                   *   bytes within word on input to
                                   *   SAHARA) */
    HAB_SAHARA_IN_SWAP16 = 0x02,  /**< Set MESS HALF WORD SWAP bit (reverse
                                   *   message half-words within word on input
                                   *   to SAHARA)  */
    /* no SWAP32 for SAHARA message - leave 0x04 value unassigned */
    /* no SWAP8 for SAHARA descriptors/links - leave 0x08 value unassigned */
    HAB_SAHARA_DSC_BE8_16 = 0x10, /**< Interpret descriptors and links as for
                                   *   BE-8 16-bit memory. */
    HAB_SAHARA_DSC_BE8_32 = 0x20  /**< Interpret descriptors and links as for
                                   *   BE-8 32-bit memory. */
} hab_sahara_config_t;

/** CAAM configuration flags 
 * @ingroup caam
 */
typedef enum hab_caam_config {
    HAB_CAAM_IN_SWAP8 = 0x01,   /**< Set Message Byte Swap Input bit (reverse
                                 *   message bytes within word on input to
                                 *   CAAM) */
    HAB_CAAM_IN_SWAP16 = 0x02,  /**< Set Message Half Word Swap Input bit
                                 *   (reverse message half-words within word
                                 *   on input to CAAM)  */
    /* no SWAP32 for CAAM message - leave 0x04 value unassigned */
    HAB_CAAM_OUT_SWAP8 = 0x08,   /**< Set Message Byte Swap Output bit
                                  *   (reverse message bytes within word on
                                  *   output from CAAM) */
    HAB_CAAM_OUT_SWAP16 = 0x10,  /**< Set Message Half Word Swap Output bit
                                  *   (reverse message half-words within word
                                  *   on output from CAAM)  */
    /* no SWAP32 for CAAM message - leave 0x20 value unassigned */
    HAB_CAAM_DSC_SWAP8 = 0x40,   /**< Set Control Byte Swap Input/Output bits
                                  *   (reverse descriptor/link bytes within
                                  *   word on input to or output from CAAM) */
    HAB_CAAM_DSC_SWAP16 = 0x80  /**< Set Control Half Word Swap Input/Output
                                  *   bits (reverse descriptor/link half-words
                                  *   within word on input to or output from
                                  *   CAAM) */
} hab_caam_config_t;

/** CAAM unlock flags 
 * @ingroup caam
 */
typedef enum hab_caam_unlock_flag {
    HAB_CAAM_UNLOCK_MID = 0x01,   /**< Leave Job Ring and DECO master ID
                                   *   registers unlocked */
    HAB_CAAM_UNLOCK_RNG   = 0x02, /**< Leave RNG state handle 0
                                   *   uninstantiated, do not generate
                                   *   descriptor keys, do not set AES DPA
                                   *   mask, do not block state handle 0 test
                                   *   instantiation */
    HAB_CAAM_UNLOCK_MFG = 0x04    /**< Keep Manufacturing Protection Private
                                   *   key in CAAM internal memory. Without
                                   *   this key, we have to erase the key. */
} hab_caam_unlock_flag_t;

/** CAAM init flags
 * @ingroup caam
 */
typedef enum hab_caam_init_flag {
    HAB_CAAM_INIT_RNG = 0x02      /**< Instantiate RNG state handle 0,
                                   *   generate descriptor keys, set AES DPA
                                   *   mask, block state handle 0 test
                                   *   instantiation */
} hab_caam_init_flag_t;

/** SNVS unlock flags 
 * @ingroup snvs
 */
typedef enum hab_snvs_unlock_flag {
    HAB_SNVS_UNLOCK_LP_SWR = 0x01,    /**< Leave LP SW reset unlocked */
    HAB_SNVS_UNLOCK_ZMK_WRITE = 0x02  /**< Leave Zeroisable Master Key write
                                       *   unlocked */
} hab_snvs_unlock_flag_t;

/** SNVS master keys 
 * @ingroup snvs
 *
 * @remark Note that the first two master key selections are completely
 * interchangeable.
 */
typedef enum hab_snvs_keys {
    HAB_SNVS_OTPMK = 0,                 /**< OTP master key */
    HAB_SNVS_OTPMK_ALIAS = 1,           /**< OTP master key (alias) */
    HAB_SNVS_ZMK = 2,                   /**< Zeroisable master key */
    HAB_SNVS_CMK = 3                    /**< Combined master key  */
} hab_snvs_keys_t;


/** OCOTP unlock flags 
 * @ingroup ocotp
 */
typedef enum hab_ocotp_unlock_flag {
    HAB_OCOTP_UNLOCK_FIELD_RETURN = 0x01, /**< Leave Field Return activation
                                           *   unlocked */
    HAB_OCOTP_UNLOCK_SRK_REVOKE = 0x02,   /**< Leave SRK revocation unlocked */
    HAB_OCOTP_UNLOCK_SCS = 0x04,          /**< Leave SCS register unlocked */
    HAB_OCOTP_UNLOCK_JTAG = 0x08          /**< Unlock JTAG using SCS HAB_JDE
                                           *   bit */
} hab_ocotp_unlock_flag_t;

/** DCP configuration flags 
 * @ingroup dcp
 *
 * @warning The byte-swapping controls produce unpredictable results unless
 * the input data block lengths are multiples of 4 bytes.
 */
typedef enum hab_dcp_config {
    HAB_DCP_IN_SWAP8 = 0x01,    /**< Set INPUT BYTE SWAP bit (reverse bytes
                                 *   within words on input to DCP) */
    /* no SWAP16 for DCP - leave 0x02 value unassigned */
    HAB_DCP_IN_SWAP32 = 0x04,   /**< Set INPUT WORD SWAP bit (ignored for
                                 *   hashing)  */
    HAB_DCP_OUT_SWAP8 = 0x08,   /**< Set OUPUT BYTE SWAP bit (reverse bytes
                                 *   within words on output from DCP) */
    /* no SWAP16 for DCP - leave 0x10 value unassigned */
    HAB_DCP_OUT_SWAP32 = 0x20   /**< Set OUTPUT WORD SWAP bit (ignored for
                                 *   hashing)  */
} hab_dcp_config_t;

/** Variable configuration items
 * @ingroup cmd_set
 */
typedef enum hab_var_cfg_itm {
    HAB_VAR_CFG_ITM_MID = 0x01, /**< Manufacturing ID (MID) fuse locations */
    HAB_VAR_CFG_ITM_ENG = 0x03  /**< Preferred engine for a given algorithm */
} hab_var_cfg_itm_t;

/*===========================================================================
                    STRUCTURES AND OTHER TYPEDEFS
=============================================================================*/

/** Header field components
 * @ingroup hdr
 */
typedef struct hab_hdr {
    uint8_t tag;              /**< Tag field */
    uint8_t len[2];           /**< Length field in bytes (big-endian) */
    uint8_t par;              /**< Parameters field */
} hab_hdr_t;

/*===========================================================================
                     GLOBAL VARIABLE DECLARATIONS
=============================================================================*/

/*===========================================================================
                         FUNCTION PROTOTYPES
=============================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* HAB_TYPES_H */
