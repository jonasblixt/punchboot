/**
 * BPAK - Bit Packer
 *
 * Copyright (C) 2019 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BPAK_BPAK_H_
#define INCLUDE_BPAK_BPAK_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BPAK_HEADER_MAGIC 0x42504132
#define BPAK_MAX_PARTS 32
#define BPAK_MAX_META 32
#define BPAK_METADATA_BYTES 1920
#define BPAK_PART_ALIGN 512
#define BPAK_META_ALIGN 8
#define BPAK_SIGNATURE_MAX_BYTES 512

enum bpak_hash_kind
{
    BPAK_HASH_INVALID,
    BPAK_HASH_SHA256,
    BPAK_HASH_SHA384,
    BPAK_HASH_SHA512,
};

enum bpak_signature_kind
{
    BPAK_SIGN_INVALID,
    BPAK_SIGN_RSA4096,
    BPAK_SIGN_PRIME256v1,
    BPAK_SIGN_SECP384r1,
    BPAK_SIGN_SECP521r1,
};

enum bpak_errors
{
    BPAK_OK,
    BPAK_FAILED,
    BPAK_NOT_FOUND,
    BPAK_SIZE_ERROR,
    BPAK_NO_SPACE_LEFT,
    BPAK_BAD_ALIGNMENT,
    BPAK_SEEK_ERROR,
    BPAK_NOT_SUPPORTED,
};

enum bpak_header_pos
{
    BPAK_HEADER_POS_FIRST,
    BPAK_HEADER_POS_LAST,
};

/* Data within this part is not included in hashing context */
#define BPAK_FLAG_EXCLUDE_FROM_HASH (1 << 0)

/*
 * Part data stream prepared for transport. When this bit is set the following
 *  metadata must be included:
 *
 * bpak-transport (struct bpak_transport_meta)
 *
 */
#define BPAK_FLAG_TRANSPORT (1 << 1)

/* Bits 2 - 7 are reserved */

/* 32 bytes */
struct bpak_transport_meta
{
    uint32_t alg_id_encode; /* Algorithm Encoder/Decoder ID */
    uint32_t alg_id_decode;
    uint8_t data[24];       /* Algorithm specific data */
} __attribute__ ((packed));

/* 32 byte */
struct bpak_part_header
{
    uint32_t id;             /* Part identifier */
    uint64_t size;           /* Data block size*/
    uint64_t offset;         /* Offset in data stream */
    uint64_t transport_size; /* Should be populated when part data is
                                prepared for transport. With the encoded
                                size */
    uint16_t pad_bytes;      /* Part padding up to the next 128 byte boundary*/
    uint8_t flags;           /* Flags */
    uint8_t pad;             /* Pad to 32 bytes, set to zero */
} __attribute__ ((packed));

/* 16 byte */
struct bpak_meta_header
{
    uint32_t id;            /* Metadata identifier */
    uint16_t size;          /* Size of metadata */
    uint16_t offset;        /* Offset in 'metadata' byte array */
    uint32_t part_id_ref;   /* Optional reference to a part id */
    uint8_t pad[4];         /* Pad to 16 bytes */
} __attribute__ ((packed));

/* 4 kBytes */
struct bpak_header
{
    uint32_t magic;
    uint8_t pad0[4];
    struct bpak_meta_header meta[BPAK_MAX_META];
    struct bpak_part_header parts[BPAK_MAX_PARTS];
    uint8_t metadata[BPAK_METADATA_BYTES]; /* Ensure that metadata array */
    uint8_t hash_kind;                     /* is aligned to a BPAK_META_ALIGN */
    uint8_t signature_kind;                /*  boundary */
    uint16_t alignment;
    uint8_t payload_hash[64];
    uint32_t key_id;
    uint32_t keystore_id;
    uint8_t pad1[42];
    uint8_t signature[BPAK_SIGNATURE_MAX_BYTES];
    uint16_t signature_sz;
} __attribute__ ((packed));

#define BPAK_MIN(__a, __b) (((__a) > (__b))?(__b):(__a))

#define bpak_foreach_part(__hdr, __var) \
    for (struct bpak_part_header *__var = (__hdr)->parts; \
       __var != &((__hdr)->parts[BPAK_MAX_PARTS]); __var++)

#define bpak_foreach_meta(__hdr, __var) \
     for (struct bpak_meta_header *__var = (__hdr)->meta; \
       __var != &((__hdr)->meta[BPAK_MAX_META]); __var++)


/*
 * Retrive pointer to metadata with id 'id'. If *ptr equals NULL
 *  the function will search from the beginning of the header array.
 *  When *ptr points to some data within the metadata block, the function
 *  will search forward from that point.
 *
 * 'ptr' pointer is assigned to the location of the metadata within
 *  the hdr->metadata byte array.
 *
 * Returns: BPAK_OK on success
 *         -BPAK_NOT_FOUND if the metadata is missing
 *
 **/

int bpak_get_meta(struct bpak_header *hdr, uint32_t id, void **ptr);

/* Get pointer to meta data with 'id' and a part reference id 'part_id_ref' */
int bpak_get_meta_with_ref(struct bpak_header *hdr, uint32_t id,
                            uint32_t part_id_ref, void **ptr);

/* Get pointer to both the metadata header and the actual data */
int bpak_get_meta_and_header(struct bpak_header *hdr, uint32_t id,
          uint32_t part_id_ref, void **ptr, struct bpak_meta_header **header);

/*
 * Add new metadata with id 'id' of size 'size'. *ptr is assigned
 * to a pointer within the hdr->metadata byte array.
 *
 * Returns:
 *          BPAK_OK on success
 *         -BPAK_NO_SPACE if the metadata array is full
 **/

int bpak_add_meta(struct bpak_header *hdr, uint32_t id, uint32_t part_ref_id,
                  void **ptr, uint16_t size);

/*
 * Retrive pointer to part with id 'id'.
 *
 * part pointer is assigned to the location of the part header within
 *  the hdr->parts array.
 *
 * Returns: BPAK_OK on success
 *         -BPAK_FAILED if part_id already exists
 *         -BPAK_NOT_FOUND if the part is missing
 *
 **/

int bpak_get_part(struct bpak_header *hdr, uint32_t id,
                  struct bpak_part_header **part);

/*
 * Add new part with 'id'. *ptr is assigned to a pointer within
 * the hdr->parts array.
 *
 * Returns:
 *          BPAK_OK on success
 *         -BPAK_NO_SPACE if the array is full
 **/

int bpak_add_part(struct bpak_header *hdr, uint32_t id,
                  struct bpak_part_header **part);

/*
 * Check magic numbers in the header and check that all parts have the correct
 *  alignment.
 */

int bpak_valid_header(struct bpak_header *hdr);

/*
 * Copy the signature to '*signature' and zero out the signature area in the
 *  header.
 *
 * Signature size is returned in '*size'
 **/

int bpak_copyz_signature(struct bpak_header *header, uint8_t *signature,
                         size_t *size);

/*
 * Initialize empty header structure
 */
int bpak_init_header(struct bpak_header *hdr);

/*
 * Get data offset of 'part' within the BPAK stream
 */
uint64_t bpak_part_offset(struct bpak_header *h, struct bpak_part_header *part);

/*
 * Get size of 'part'
 *
 * Returns:
 *          Data size + padding bytes or transport size without padding
 *           depending on if the transport bit is set in part->flags
 */
uint64_t bpak_part_size(struct bpak_part_header *part);

/* Translate error codes to string representation */
const char *bpak_error_string(int code);


/* Return string representaion of known id's */
const char *bpak_known_id(uint32_t id);
const char *bpak_signature_kind(uint8_t signature_kind);
const char *bpak_hash_kind(uint8_t hash_kind);

int bpak_printf(int verbosity, const char *fmt, ...);

const char *bpak_version(void);

#endif  // INCLUDE_BPAK_BPAK_H_
