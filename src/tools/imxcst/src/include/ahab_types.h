/*
 *
 *     Copyright 2017-2019 NXP
 *
 *     Redistribution and use in source and binary forms, with or without modification,
 *     are permitted provided that the following conditions are met:
 *
 *     o Redistributions of source code must retain the above copyright notice, this list
 *       of conditions and the following disclaimer.
 *
 *     o Redistributions in binary form must reproduce the above copyright notice, this
 *       list of conditions and the following disclaimer in the documentation and/or
 *       other materials provided with the distribution.
 *
 *     o Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *     WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *     ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *     ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *     (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *     SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef AHAB_TYPES_H
#define AHAB_TYPES_H

#include "bitops.h"

/*
 * Definitions applicable for all cryptography related headers:
 *  - certificate
 *  - signature
 *  - Super Root Key
 *  - image (hashing)
 */
#define SRK_RSA     0x21
#define SRK_ECDSA   0x27

#define SRK_PRIME256V1  0x1
#define SRK_SEC384R1    0x2
#define SRK_SEC521R1    0x3
#define SRK_RSA2048     0x5
#define SRK_RSA3072     0x6
#define SRK_RSA4096     0x7

#define SRK_SHA256  0x0
#define SRK_SHA384  0x1
#define SRK_SHA512  0x2

#define MAX_SRK_SHA_TYPE SRK_SHA512

struct __attribute__ ((packed)) ahab_container_image_s {
    /* offset 0x0 */
    uint32_t image_offset;
    /* offset 0x4 */
    uint32_t image_size;
    /* offset 0x8 */
    uint64_t load_address;
    /* offset 0x10 */
    uint64_t entry_point;
    /* offset 0x18 */
    uint32_t flags;
    /* offset 0x1c */
    uint32_t reserved;
    /* offset 0x20 */
    uint8_t hash[64];
    /* offset 0x60 */
    uint8_t iv[32];
};

#define IMAGE_FLAGS_TYPE_SHIFT          0
#define IMAGE_FLAGS_TYPE_MASK           (BIT_MASK(3, 0))
# define IMAGE_FLAGS_TYPE_EXECUTABLE    0x3
# define IMAGE_FLAGS_TYPE_DATA          0x4
# define IMAGE_FLAGS_TYPE_DCD           0x5
# define IMAGE_FLAGS_TYPE_SECO          0x6
# define IMAGE_FLAGS_TYPE_PROVISIONING  0x7
# define IMAGE_FLAGS_TYPE_DEK           0x8
static inline uint8_t ahab_container_image_get_type(struct ahab_container_image_s *hdr)
{
    return bf_get_uint8(hdr->flags, IMAGE_FLAGS_TYPE_MASK, IMAGE_FLAGS_TYPE_SHIFT);
}

#define IMAGE_FLAGS_CORE_ID_SHIFT   4
#define IMAGE_FLAGS_CORE_ID_MASK    (BIT_MASK(7, 4))

#define IMAGE_CORE_ID_SC         1
#define IMAGE_CORE_ID_CM4_0      2
#define IMAGE_CORE_ID_CM4_1      3
#define IMAGE_CORE_ID_CA53       4
#define IMAGE_CORE_ID_CA35       4
#define IMAGE_CORE_ID_CA72       5
#define IMAGE_CORE_ID_SECO       6

#define IMAGE_FLAGS_HASH_SHIFT      8
#define IMAGE_FLAGS_HASH_MASK       (BIT_MASK(10, 8))
static inline uint8_t ahab_container_image_get_hash(struct ahab_container_image_s *hdr)
{
    return bf_get_uint8(hdr->flags, IMAGE_FLAGS_HASH_MASK, IMAGE_FLAGS_HASH_SHIFT);
}

#define IMAGE_FLAGS_ENCRYPTED BIT(11)

struct __attribute__ ((packed)) ahab_container_header_s {
    /* offset 0x0*/
    uint8_t version;
    uint16_t length;
    uint8_t tag;
    /* offset 0x4*/
    uint32_t flags;
    /* offset 0x8*/
    uint16_t sw_version;
    uint8_t fuse_version;
    uint8_t nrImages;
    /* offset 0xc*/
    uint16_t signature_block_offset;
    uint16_t reserved;
};

static inline struct ahab_container_image_s *
get_ahab_image_array(struct ahab_container_header_s *header)
{
    return (struct ahab_container_image_s *)(uintptr_t)
           ((uint32_t)(uintptr_t) header + sizeof(struct ahab_container_header_s));
}

#define HEADER_FLAGS_SRK_SET_SHIFT  0
#define HEADER_FLAGS_SRK_SET_MASK   (BIT_MASK(1, 0))
# define HEADER_FLAGS_SRK_SET_NONE  0x0
# define HEADER_FLAGS_SRK_SET_NXP   0x1
# define HEADER_FLAGS_SRK_SET_OEM   0x2

#define HEADER_FLAGS_SRK_SHIFT  4
#define HEADER_FLAGS_SRK_MASK   (BIT_MASK(5, 4))

#define HEADER_FLAGS_REVOKING_MASK_SHIFT 8
#define HEADER_FLAGS_REVOKING_MASK_MASK (BIT_MASK(11, 8))

struct __attribute__ ((packed)) ahab_container_signature_block_s {
    /* offset 0x0 */
    uint8_t version;
    uint16_t length;
    uint8_t tag;
    /* offset 0x4 */
    uint16_t certificate_offset;
    uint16_t srk_table_offset;
    /* offset 0x8 */
    uint16_t signature_offset;
    uint16_t blob_offset;
    /* offset 0xc */
    uint32_t key_identifier;
    /* offset 0x10 */
};

#define SIGNATURE_BLOCK_TAG 0x90

#define SRK_TAG         0xE1
#define SRK_FLAGS_CA    0x80

struct __attribute__ ((packed)) ahab_container_srk_s {
    /* offset 0x0 */
    uint8_t tag;
    uint16_t length;
    uint8_t encryption;
    /* offset 0x4 */
    uint8_t hash;
    uint8_t key_size_or_curve;
    uint8_t reserved;
    uint8_t flags;
    /* offset 0x8 */
    uint16_t modulus_or_x_length;
    uint16_t exponent_or_y_length;
};

#define SRK_TABLE_TAG       0xD7
#define SRK_TABLE_VERSION   0x42
struct __attribute__ ((packed)) ahab_container_srk_table_s {
    /* offset 0x0 */
    uint8_t tag;
    uint16_t length;
    uint8_t version;
    /* offset 0x4 */
};

#define SIGNATURE_TAG 0xD8
struct __attribute__ ((packed)) ahab_container_signature_s {
    /* offset 0x0 */
    uint8_t version;
    uint16_t length;
    uint8_t tag;
    /* offset 0x4 */
    uint32_t reserved;
    /* offset 0x8 */
    uint8_t data[];
};

static inline int32_t ahab_get_hash_size_by_sha_type(uint8_t hash)
{
    if (hash > MAX_SRK_SHA_TYPE) {
        return -1;
    }

    return (32 + (16 * hash));
}

#define CERTIFICATE_TAG 0xAF
struct __attribute__ ((packed)) ahab_container_certificate_s {
    /* offset 0x0 */
    uint8_t version;
    uint16_t length;
    uint8_t tag;
    /* offset 0x4 */
    uint16_t signature_offset;
    uint16_t permissions;
    /* offset 0x8 */
    struct ahab_container_srk_s public_key;
};

/* Public declarations and definitions */
#define AHAB_MAX_NR_IMAGES 32

typedef enum ahab_hash {
    HASH_SHA256 = SRK_SHA256,
    HASH_SHA384 = SRK_SHA384,
    HASH_SHA512 = SRK_SHA512,
} ahab_hash_t;

#define AHAB_BLOB_HEADER          8
#define CAAM_BLOB_OVERHEAD_NORMAL 48

#endif /* AHAB_TYPES_H */
