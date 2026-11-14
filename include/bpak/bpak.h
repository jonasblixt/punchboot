/**
 * \file bpak.h
 *
 * BPAK - Bit Packer
 *
 * Copyright (C) 2022 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BPAK_BPAK_H_
#define INCLUDE_BPAK_BPAK_H_

#include <bpak/version.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef BPAK_HAVE_USER_SETTINGS
#include "bpak_user_settings.h"
#else
#include <bpak/build_config.h>
#endif

#ifdef BPAK_SIZEOF_OFF_T
/* off_t is part of the ABI, a program must be compiled with the
 * _FILE_OFFSET_BITS that the library was built with. The array has a negative
 * size, and the compilation fails, when off_t has another size. */
typedef char bpak_off_t_has_the_size_of_the_library[(sizeof(off_t) == BPAK_SIZEOF_OFF_T) ? 1 : -1];
#endif

#define BPAK_EXPORT __attribute__((visibility("default")))

/* Lets the compiler check the arguments of printf like functions */
#if defined(__GNUC__)
#define BPAK_PRINTF_FORMAT(_fmt, _args) __attribute__((format(printf, _fmt, _args)))
#else
#define BPAK_PRINTF_FORMAT(_fmt, _args)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \def BPAK_HEADER_MAGIC
 * Magic number for BPAK header identification
 *
 * \def BPAK_MAX_PARTS
 * Maximum number of data parts a package can have
 *
 * \def BPAK_MAX_META
 * Maximum number of metadata fields a package can have
 *
 * \def BPAK_METADATA_BYTES
 * Maximum number of bytes allocated for metadata
 *
 * \def BPAK_PART_ALIGN
 * Alignment of data parts, parts are zero padded to a multiple of this
 *
 * \def BPAK_META_ALIGN
 * Alignment of the meta data values in the 'metadata' array
 *
 * \def BPAK_SIGNATURE_MAX_BYTES
 * Maximum size of signature
 *
 * \def BPAK_HASH_MAX_LENGTH
 * Size of the largest hash, sha512
 *
 * \def BPAK_CHUNK_BUFFER_LENGTH
 * Size of the buffers that part data is read with, can be overridden
 *
 **/

#define BPAK_HEADER_MAGIC        0x42504132
#define BPAK_MAX_PARTS           32
#define BPAK_MAX_META            32
#define BPAK_METADATA_BYTES      1920
#define BPAK_PART_ALIGN          512
#define BPAK_META_ALIGN          8
#define BPAK_SIGNATURE_MAX_BYTES 512
#define BPAK_HASH_MAX_LENGTH     64

#ifndef BPAK_CHUNK_BUFFER_LENGTH
#define BPAK_CHUNK_BUFFER_LENGTH 4096
#endif

/*! \public
 *
 * BPAK Hash identifier
 */
enum bpak_hash_kind {
    BPAK_HASH_INVALID,
    BPAK_HASH_SHA256,
    BPAK_HASH_SHA384,
    BPAK_HASH_SHA512,
};

/*! \public
 *
 * BPAK signature format
 */
enum bpak_signature_kind {
    BPAK_SIGN_INVALID,
    BPAK_SIGN_RSA4096,
    BPAK_SIGN_PRIME256v1,
    BPAK_SIGN_SECP384r1,
    BPAK_SIGN_SECP521r1,
};

/*! \public
 *
 * BPAK Result codes. Functions return BPAK_OK or a negated error code.
 */
enum bpak_errors {
    BPAK_OK,
    BPAK_FAILED,
    BPAK_NOT_FOUND,
    BPAK_SIZE_ERROR,
    BPAK_NO_SPACE_LEFT,
    BPAK_BAD_ALIGNMENT,
    BPAK_SEEK_ERROR,
    BPAK_NOT_SUPPORTED,
    BPAK_NEEDS_MORE_DATA,
    BPAK_WRITE_ERROR,
    BPAK_DECOMPRESSOR_ERROR,
    BPAK_COMPRESSOR_ERROR,
    BPAK_PATCH_READ_ORIGIN_ERROR,
    BPAK_PATCH_WRITE_ERROR,
    BPAK_READ_ERROR,
    BPAK_BAD_MAGIC,
    BPAK_UNSUPPORTED_HASH_ALG,
    BPAK_BUFFER_TOO_SMALL,
    BPAK_FILE_NOT_FOUND,
    BPAK_KEY_DECODE,
    BPAK_VERIFY_FAIL,
    BPAK_SIGN_FAIL,
    BPAK_UNSUPPORTED_KEY,
    BPAK_BAD_ROOT_HASH,
    BPAK_BAD_PAYLOAD_HASH,
    BPAK_MISSING_META_DATA,
    BPAK_PACKAGE_UUID_MISMATCH,
    BPAK_UNSUPPORTED_COMPRESSION,
    BPAK_KEY_NOT_FOUND,
    BPAK_KEYSTORE_ID_MISMATCH,
    BPAK_EXISTS,
};

/*! \public
 *
 * BPAK Compression methods
 */
enum bpak_compression {
    BPAK_COMPRESSION_NONE,
    BPAK_COMPRESSION_HS,
    BPAK_COMPRESSION_LZMA,
};

/*! \public
 *
 * BPAK key types
 */
enum bpak_key_kind {
    BPAK_KEY_INVALID,
    BPAK_KEY_PUB_RSA4096,
    BPAK_KEY_PUB_PRIME256v1,
    BPAK_KEY_PUB_SECP384r1,
    BPAK_KEY_PUB_SECP521r1,
    BPAK_KEY_PRI_RSA4096,
    BPAK_KEY_PRI_PRIME256v1,
    BPAK_KEY_PRI_SECP384r1,
    BPAK_KEY_PRI_SECP521r1,
};

/**
 * \def BPAK_FLAG_EXCLUDE_FROM_HASH
 * The data of this part is not included in the payload hash
 *
 **/
#define BPAK_FLAG_EXCLUDE_FROM_HASH (1 << 0)

/**
 * \def BPAK_FLAG_TRANSPORT
 * The part data is transport encoded and its size is 'transport_size'. When
 *  this bit is set the following metadata must be included:
 *
 * bpak-transport (struct bpak_transport_meta)
 *
 */
#define BPAK_FLAG_TRANSPORT         (1 << 1)

/* Bits 2 - 7 are reserved */

/**
 * Numerical ID representing different types of objects
 *
 * Size: 4 bytes
 */
typedef uint32_t bpak_id_t;

/**
 * Transport mode meta data
 *
 * Size: 32 bytes
 **/
struct bpak_transport_meta {
    uint32_t alg_id_encode; /*!< Algorithm encoder ID */
    uint32_t alg_id_decode; /*!< Algorithm decoder ID */
    uint8_t data[24]; /*!< Algorithm specific data */
} __attribute__((packed));

/**
 * I/O callback for reading or writing 'length' bytes at 'offset'
 *
 * @return The number of bytes read or written or a negative error code
 */
typedef ssize_t (*bpak_io_t)(off_t offset, uint8_t *buffer, size_t length, void *user);

/** Allocator with the signature of calloc(), see bpak_set_calloc_free() */
typedef void *(*bpak_calloc_t)(size_t, size_t);

/** Deallocator with the signature of free(), see bpak_set_calloc_free() */
typedef void (*bpak_free_t)(void *);

/**
 * BPAK part header
 *
 * Size:32 byte
 **/
struct bpak_part_header {
    bpak_id_t id; /*!< Part identifier */
    uint64_t size; /*!< Data block size*/
    uint64_t offset; /*!< Offset of the data in the decoded
                        package, including the header. Not used
                        by the library, which computes the
                        offsets with bpak_part_offset() */
    uint64_t transport_size; /*!< Should be populated when part data is
                                prepared for transport. With the encoded
                                size */
    uint16_t pad_bytes; /*!< Zero padding up to the next BPAK_PART_ALIGN
                             (512) byte boundary */
    uint8_t flags; /*!< Flags */
    uint8_t pad; /*!< Pad to 32 bytes, set to zero */
} __attribute__((packed));

/**
 * BPAK Meta data header
 *
 * Size: 16 byte
 **/
struct bpak_meta_header {
    bpak_id_t id; /*!< Metadata identifier */
    uint16_t size; /*!< Size of metadata */
    uint16_t offset; /*!< Offset in 'metadata' byte array */
    bpak_id_t part_id_ref; /*!< Optional reference to a part id */
    uint8_t pad[4]; /*!< Pad to 16 bytes */
} __attribute__((packed));

/**
 * BPAK Header
 *
 *
 * Size: 4 kBytes
 **/
struct bpak_header {
    uint32_t magic; /*!< BPAK Magic number*/
    uint8_t pad0[4]; /*!< Pad 1*/
    struct bpak_meta_header meta[BPAK_MAX_META]; /*!< Meta data header array */
    struct bpak_part_header parts[BPAK_MAX_PARTS]; /*!< Part header array */
    uint8_t metadata[BPAK_METADATA_BYTES]; /*!< Meta data byte array */
    uint8_t hash_kind; /*!< Hash kind used in package */
    uint8_t signature_kind; /*!< Signature kind used in package, set by
                                 bpak_pkg_sign(). Verification uses the
                                 kind of the key, not this field */
    uint16_t alignment; /*!< Alignment */
    uint8_t payload_hash[64]; /*!< Payload hash */
    uint32_t key_id; /*!< Signing key identifier */
    uint32_t keystore_id; /*!< Keystore identifier */
    uint8_t pad1[42]; /*!< Pad 2 */
    uint8_t signature[BPAK_SIGNATURE_MAX_BYTES]; /*!< Signature data */
    uint16_t signature_sz; /*!< Signature size */
} __attribute__((packed));

/**
 * \def BPAK_MIN
 * Helper macro to return the smaller of two values
 */

#define BPAK_MIN(_a, _b) (((_a) > (_b)) ? (_b) : (_a))

/**
 * \def bpak_foreach_part
 *
 * Helper macro to iterate over all part slots in a package. The parts are
 * followed by empty slots, an empty slot (id 0) ends the parts, see
 * bpak_valid_header().
 */
#define bpak_foreach_part(_hdr, _var)                                                             \
    for (struct bpak_part_header *_var = (_hdr)->parts; _var != &((_hdr)->parts[BPAK_MAX_PARTS]); \
         _var++)

/**
 * \def bpak_foreach_meta
 *
 * Helper macro to iterate over all metadata in a package
 */
#define bpak_foreach_meta(_hdr, _var)                                                          \
    for (struct bpak_meta_header *_var = (_hdr)->meta; _var != &((_hdr)->meta[BPAK_MAX_META]); \
         _var++)

/**
 * \def bpak_get_meta_ptr
 *
 * Helper macro to get a pointer of type '_T *' to the value of the metadata
 *  '__meta' in the header '_hdr'
 */
#define bpak_get_meta_ptr(_hdr, __meta, _T) ((_T *)&(_hdr)->metadata[(__meta)->offset])

/**
 * Get pointer to metadata header with 'id' and a part reference id 'part_id_ref'.
 *
 * @param[in] hdr BPAK Header
 * @param[in] id Meta data identifier
 * @param[in] part_id_ref Part reference identifier, or 0 for no reference
 * @param[out] meta Pointer to the metadata header
 *
 * @return BPAK_OK on success -BPAK_NOT_FOUND if the metadata is missing or
 *         'id' is 0
 *
 **/
int bpak_get_meta(struct bpak_header *hdr,
                  bpak_id_t id,
                  bpak_id_t part_id_ref,
                  struct bpak_meta_header **meta);

/**
 * Get pointer to metadata header with 'id' and any part reference id
 *
 * @param[in] hdr BPAK Header
 * @param[in] id Meta data identifier
 * @param[out] meta Pointer to the metadata header
 *
 * @return BPAK_OK on success -BPAK_NOT_FOUND if the metadata is missing or
 *         'id' is 0
 *
 **/
int bpak_get_meta_anyref(struct bpak_header *hdr, bpak_id_t id, struct bpak_meta_header **meta);

/**
 * Add new metadata with id 'id' of size 'size'. *meta is assigned
 * to a pointer within the hdr->meta header array.
 *
 * @param[in] hdr BPAK Header
 * @param[in] id ID of new metadata
 * @param[in] part_ref_id Optional part reference to use for new metadata
 * @param[in] size Size in bytes of new metadata
 * @param[out] meta Pointer to the metadata header
 *
 * @return BPAK_OK on success,
 *         -BPAK_NO_SPACE_LEFT if the metadata array is full or
 *         -BPAK_EXISTS if a metadata with the same 'id' and 'part_ref_id'
 *         already exists
 **/
int bpak_add_meta(struct bpak_header *hdr,
                  bpak_id_t id,
                  bpak_id_t part_ref_id,
                  uint16_t size,
                  struct bpak_meta_header **meta);

/**
 * Remove metadata pointed to by 'meta'.
 *
 * @param[in] hdr BPAK Header
 * @param[in] meta Pointer to metadata header. Assumed to be within hdr->meta array
 *
 **/
void bpak_del_meta(struct bpak_header *hdr, struct bpak_meta_header *meta);

/**
 * Retrieve pointer to part with id 'id'.
 *
 * part pointer is assigned to the location of the part header within
 *  the hdr->parts array.
 *
 * @param[in] hdr BPAK Header
 * @param[in] id ID of part
 * @param[out] part Pointer to the part header
 *
 * @return BPAK_OK on success, -BPAK_NOT_FOUND if the part is missing or 'id'
 *         is 0
 *
 **/

int bpak_get_part(struct bpak_header *hdr, bpak_id_t id, struct bpak_part_header **part);

/**
 * Add new part with 'id'. *ptr is assigned to a pointer within
 * the hdr->parts array.
 *
 * @param[in] hdr BPAK Header
 * @param[in] id ID of new part
 * @param[out] part Output pointer to new part
 *
 * @return BPAK_OK on success,
 *         -BPAK_NO_SPACE_LEFT if the array is full
 *         -BPAK_EXISTS if a part with the same 'id' already exists
 *
 **/

int bpak_add_part(struct bpak_header *hdr, bpak_id_t id, struct bpak_part_header **part);

/**
 * Remove part pointed to by 'part'.
 *
 * @param[in] hdr BPAK Header
 * @param[in] part Pointer to part header. Assumed to be within hdr->parts array
 *
 **/
void bpak_del_part(struct bpak_header *hdr, struct bpak_part_header *part);

/**
 * Check that a header that is not verified yet can be used: the magic number,
 *  that the parts and meta data follow each other without empty slots, the
 *  alignment of the parts and the meta data values, that the meta data is
 *  within the meta data array, that the part sizes and offsets fit size_t and
 *  off_t and the signature size. The part offsets are not checked.
 *
 *  @param[in] hdr BPAK Header
 *
 *  @return BPAK_OK on success, -BPAK_BAD_MAGIC, -BPAK_BAD_ALIGNMENT,
 *          -BPAK_SIZE_ERROR or -BPAK_NOT_SUPPORTED for empty slots between
 *          parts or meta data and an invalid hash kind
 */

int bpak_valid_header(struct bpak_header *hdr);

/**
 * Copy the signature to '*signature' and zero out the signature area in the
 *  header.
 *
 * Signature size is returned in '*size'
 *
 * @param[in] hdr BPAK Header
 * @param[out] signature Signature output buffer
 * @param[in] size Size in and size out
 *
 * @return BPAK_OK On success
 **/

int bpak_copyz_signature(struct bpak_header *hdr, uint8_t *signature, size_t *size);

/**
 * Initialize empty header structure
 *
 * @param[in] hdr BPAK Header
 *
 * @return BPAK_OK on success
 */
int bpak_init_header(struct bpak_header *hdr);

/**
 * Get data offset of 'part' within the BPAK stream. This includes
 * the header (4kByte).
 *
 * @param[in] h BPAK Header
 * @param[in] part BPAK Part pointer
 *
 * @return Offset in bytes
 */
off_t bpak_part_offset(struct bpak_header *h, struct bpak_part_header *part);

/**
 * Get size of 'part'
 *
 * @param[in] part BPAK Part
 *
 * @return  Data size + padding bytes or transport size without padding
 *           depending on if the transport bit is set in part->flags
 */
size_t bpak_part_size(struct bpak_part_header *part);

/**
 * Get size of 'part' without padding
 *
 * @param[in] part BPAK Part
 *
 * @return  Data size bytes or transport size without padding
 *           depending on if the transport bit is set in part->flags
 */
size_t bpak_part_size_wo_pad(struct bpak_part_header *part);

/**
 * Translate error codes to string representation
 *
 * @param[in] code Input error code
 *
 * @return Textual representation of error code
 **/
const char *bpak_error_string(int code);

/**
 * Translates signature type to a textual version
 *
 * @param[in] signature_kind Signature kind
 *
 * @return Textual version of signature kind
 **/
const char *bpak_signature_kind(uint8_t signature_kind);

/**
 * Translate hash kind to a textual version
 *
 * @param[in] hash_kind Hash kind
 *
 * @return Textual version of hash kind
 **/
const char *bpak_hash_kind(uint8_t hash_kind);

/**
 * Log function of the library. The default implementation is a weak symbol
 * that discards all messages, applications define their own to print them.
 *
 * @param[in] verbosity Verbosity level 0 (errors) to 2 (debug)
 * @param[in] fmt printf format string
 *
 * @return BPAK_OK on success
 */
int bpak_printf(int verbosity, const char *fmt, ...) BPAK_PRINTF_FORMAT(2, 3);

/**
 * Sets key id which is used when verifying the package as a key hint to select
 *  the correct verification key.
 *
 * @param[in] hdr BPAK Header
 * @param[in] key_id The key ID
 *
 * @return BPAK_OK on success
 */
int bpak_set_key_id(struct bpak_header *hdr, uint32_t key_id);

/**
 * Set keystore ID which is used when verifying the package. The key pointed to
 *  by the 'key_id' is expected to exist in a keystore with id 'keystore_id'
 *
 * @param[in] hdr BPAK Header
 * @param[in] keystore_id The keystore ID
 *
 * @return BPAK_OK on success
 */
int bpak_set_keystore_id(struct bpak_header *hdr, uint32_t keystore_id);

/**
 * Add transport meta data to the header. 'encoder_id' and 'decoder_id'
 * specifies which encoders/decoders should be used when encoding or decoding
 * part with id 'part_id'.
 *
 * @param[in] header BPAK Header
 * @param[in] part_id Id of part
 * @param[in] encoder_id Encoder id
 * @param[in] decoder_id Decoder id
 *
 * @return BPAK_OK on success
 */
int bpak_add_transport_meta(struct bpak_header *header,
                            bpak_id_t part_id,
                            uint32_t encoder_id,
                            uint32_t decoder_id);
/**
 * Library version
 *
 * @return Library version as a text string
 **/
const char *bpak_version(void);

/**
 * Override the default allocator
 *
 * @param[in] calloc_func Override calloc
 * @param[in] free_func Override free
 *
 * @return BPAK_OK on success,
 */
int bpak_set_calloc_free(bpak_calloc_t calloc_func, bpak_free_t free_func);

/**
 * Allocate zeroed memory with the allocator set by bpak_set_calloc_free()
 */
void *bpak_calloc(size_t nmemb, size_t size);

/**
 * Free memory from bpak_calloc()
 */
void bpak_free(void *ptr);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // INCLUDE_BPAK_BPAK_H_
