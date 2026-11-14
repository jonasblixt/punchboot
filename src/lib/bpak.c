/**
 * BPAK - Bit Packer
 *
 * Copyright (C) 2022 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <bpak/bpak.h>
#include <bpak/id.h>
#include <string.h>

#include "internal.h"

/* BPAK_META_ALIGN is a power-of-2 */
#define BPAK_META_ALIGN_SIZE(_x) (((_x) + ((BPAK_META_ALIGN) - 1)) & ~((BPAK_META_ALIGN) - 1))

static int bpak_get_meta_int(struct bpak_header *hdr,
                             bpak_id_t id,
                             bpak_id_t part_id_ref,
                             bool match_partref,
                             struct bpak_meta_header **meta)
{
    /* Id 0 is an empty slot */
    if (!id) {
        *meta = NULL;
        return -BPAK_NOT_FOUND;
    }

    bpak_foreach_meta(hdr, m) {
        if (m->id == id) {
            if (match_partref) {
                if (m->part_id_ref != part_id_ref)
                    continue;
            }

            *meta = m;

            return BPAK_OK;
        }
    }

    *meta = NULL;
    return -BPAK_NOT_FOUND;
}

BPAK_EXPORT int bpak_get_meta(struct bpak_header *hdr,
                              bpak_id_t id,
                              bpak_id_t part_id_ref,
                              struct bpak_meta_header **meta)
{
    return bpak_get_meta_int(hdr, id, part_id_ref, true, meta);
}

BPAK_EXPORT int bpak_get_meta_anyref(struct bpak_header *hdr,
                                     bpak_id_t id,
                                     struct bpak_meta_header **meta)
{
    return bpak_get_meta_int(hdr, id, 0, false, meta);
}

int bpak_get_meta_value(struct bpak_header *hdr,
                        bpak_id_t id,
                        bpak_id_t part_id_ref,
                        size_t min_size,
                        size_t max_size,
                        uint8_t **value)
{
    struct bpak_meta_header *meta;
    int rc = bpak_get_meta(hdr, id, part_id_ref, &meta);

    if (rc != BPAK_OK)
        return rc;

    if (meta->size < min_size || meta->size > max_size)
        return -BPAK_SIZE_ERROR;

    *value = bpak_get_meta_ptr(hdr, meta, uint8_t);
    return BPAK_OK;
}

int bpak_same_package_uuid(struct bpak_header *a, struct bpak_header *b)
{
    uint8_t *uuid_a;
    uint8_t *uuid_b;
    int rc;

    rc = bpak_get_meta_value(a, BPAK_ID_BPAK_PACKAGE, 0, 16, SIZE_MAX, &uuid_a);

    if (rc != BPAK_OK)
        return rc;

    rc = bpak_get_meta_value(b, BPAK_ID_BPAK_PACKAGE, 0, 16, SIZE_MAX, &uuid_b);

    if (rc != BPAK_OK)
        return rc;

    if (memcmp(uuid_a, uuid_b, 16) != 0)
        return -BPAK_PACKAGE_UUID_MISMATCH;

    return BPAK_OK;
}

BPAK_EXPORT int bpak_add_meta(struct bpak_header *hdr,
                              bpak_id_t id,
                              bpak_id_t part_ref_id,
                              uint16_t size,
                              struct bpak_meta_header **meta)
{
    uint16_t new_offset = 0;

    bpak_foreach_meta(hdr, m) {
        if (!m->id) {
            /* Check the space first, a failed add must leave the header as
             * it was */
            if ((new_offset + size) > BPAK_METADATA_BYTES)
                return -BPAK_NO_SPACE_LEFT;

            m->id = id;
            m->offset = new_offset;
            m->size = size;
            m->part_id_ref = part_ref_id;
            *meta = m;
            return BPAK_OK;
        } else if (m->id == id && m->part_id_ref == part_ref_id) {
            return -BPAK_EXISTS;
        }

        new_offset += m->size;

        new_offset = BPAK_META_ALIGN_SIZE(new_offset);
    }

    return -BPAK_NO_SPACE_LEFT;
}

BPAK_EXPORT void bpak_del_meta(struct bpak_header *hdr, struct bpak_meta_header *meta)
{
    struct bpak_meta_header *last = &hdr->meta[BPAK_MAX_META - 1];
    struct bpak_meta_header *m = meta;
    /* The values are stored in order at aligned offsets, see bpak_add_meta */
    size_t start = meta->offset;
    size_t end = meta->offset + meta->size;
    uint16_t offset_adjust = BPAK_META_ALIGN_SIZE(meta->size);

    /* Move the following meta data headers one step down */
    for (; m != last; m++) {
        memcpy(m, m + 1, sizeof(*m));

        if (!m->id)
            break;

        if ((size_t)m->offset + m->size > end)
            end = (size_t)m->offset + m->size;

        m->offset -= offset_adjust;
    }

    /* The last header was copied, or deleted when it is 'meta' */
    if (m == last)
        memset(last, 0, sizeof(*last));

    /* Move the following values down in one block, which keeps the zero
     * padding between them, and zero the end */
    end = BPAK_MIN(end, (size_t)BPAK_METADATA_BYTES);

    if (start < end) {
        size_t src = BPAK_MIN(start + offset_adjust, end);

        memmove(&hdr->metadata[start], &hdr->metadata[src], end - src);
        memset(&hdr->metadata[start + end - src], 0, src - start);
    }
}

BPAK_EXPORT int bpak_get_part(struct bpak_header *hdr, bpak_id_t id, struct bpak_part_header **part)
{
    /* Id 0 is an empty slot */
    if (!id)
        return -BPAK_NOT_FOUND;

    bpak_foreach_part(hdr, p) {
        if (p->id == id) {
            (*part) = p;
            return BPAK_OK;
        }
    }

    return -BPAK_NOT_FOUND;
}

BPAK_EXPORT int bpak_add_part(struct bpak_header *hdr, bpak_id_t id, struct bpak_part_header **part)
{
    bpak_foreach_part(hdr, p) {
        if (!p->id) {
            p->id = id;
            (*part) = p;
            return BPAK_OK;
        } else if (p->id == id) {
            return -BPAK_EXISTS;
        }
    }

    return -BPAK_NO_SPACE_LEFT;
}

BPAK_EXPORT void bpak_del_part(struct bpak_header *hdr, struct bpak_part_header *part)
{
    while (part != &(hdr->parts[BPAK_MAX_PARTS - 1])) {
        memcpy(part, part + 1, sizeof(*part));

        if (!part->id)
            break;

        part++;
    }

    memset(part, 0, sizeof(*part));
}

BPAK_EXPORT int bpak_init_header(struct bpak_header *hdr)
{
    memset(hdr, 0, sizeof(*hdr));
    hdr->magic = BPAK_HEADER_MAGIC;
    hdr->hash_kind = BPAK_HASH_SHA256;
    hdr->signature_kind = BPAK_SIGN_PRIME256v1;
    hdr->alignment = BPAK_PART_ALIGN;
    return BPAK_OK;
}

BPAK_EXPORT const char *bpak_error_string(int code)
{
    switch (code) {
    case BPAK_OK:
        return "OK";
    case -BPAK_FAILED:
        return "Failed";
    case -BPAK_NOT_FOUND:
        return "Not found";
    case -BPAK_SIZE_ERROR:
        return "Size error";
    case -BPAK_NO_SPACE_LEFT:
        return "No space left";
    case -BPAK_BAD_ALIGNMENT:
        return "Bad alignment";
    case -BPAK_SEEK_ERROR:
        return "Seek error";
    case -BPAK_NOT_SUPPORTED:
        return "Not supported";
    case -BPAK_NEEDS_MORE_DATA:
        return "Needs more data";
    case -BPAK_WRITE_ERROR:
        return "Write error";
    case -BPAK_DECOMPRESSOR_ERROR:
        return "Decompressor error";
    case -BPAK_COMPRESSOR_ERROR:
        return "Compressor error";
    case -BPAK_PATCH_READ_ORIGIN_ERROR:
        return "Patch read origin error";
    case -BPAK_PATCH_WRITE_ERROR:
        return "Patch write error";
    case -BPAK_READ_ERROR:
        return "Read error";
    case -BPAK_BAD_MAGIC:
        return "Bad magic";
    case -BPAK_UNSUPPORTED_HASH_ALG:
        return "Unsupported hash algorithm";
    case -BPAK_BUFFER_TOO_SMALL:
        return "Buffer too small";
    case -BPAK_FILE_NOT_FOUND:
        return "File not found";
    case -BPAK_KEY_DECODE:
        return "Could not decode key";
    case -BPAK_VERIFY_FAIL:
        return "Verification failed";
    case -BPAK_SIGN_FAIL:
        return "Signing failed";
    case -BPAK_UNSUPPORTED_KEY:
        return "Unsupported key";
    case -BPAK_BAD_ROOT_HASH:
        return "Bad root hash";
    case -BPAK_BAD_PAYLOAD_HASH:
        return "Bad payload hash";
    case -BPAK_MISSING_META_DATA:
        return "Missing meta data";
    case -BPAK_PACKAGE_UUID_MISMATCH:
        return "Package UUID mismatch";
    case -BPAK_UNSUPPORTED_COMPRESSION:
        return "Unsupported compression method";
    case -BPAK_KEY_NOT_FOUND:
        return "Key not found";
    case -BPAK_KEYSTORE_ID_MISMATCH:
        return "Keystore id mismatch";
    case -BPAK_EXISTS:
        return "Object exists";
    default:
        return "Unknown";
    }
}

BPAK_EXPORT int bpak_valid_header(struct bpak_header *hdr)
{
    /* Largest offset and size that off_t and size_t can hold */
    const uint64_t off_max = ((uint64_t)1 << (sizeof(off_t) * 8 - 1)) - 1;
    const uint64_t size_max = BPAK_MIN((uint64_t)SIZE_MAX, off_max);
    uint64_t stored_end = sizeof(*hdr);
    uint64_t decoded_end = sizeof(*hdr);
    bool empty = false;

    if (hdr->magic != BPAK_HEADER_MAGIC)
        return -BPAK_BAD_MAGIC;

    /* The parts follow each other without empty slots between them, the
     * lookups search all slots. The sizes and offsets must fit the API
     * types. */
    bpak_foreach_part(hdr, p) {
        if (!p->id) {
            empty = true;
            continue;
        }

        if (empty)
            return -BPAK_NOT_SUPPORTED;

        if (p->pad_bytes >= BPAK_PART_ALIGN || (bpak_part_decoded_size(p) % BPAK_PART_ALIGN) != 0)
            return -BPAK_BAD_ALIGNMENT;

        if (p->size > size_max - p->pad_bytes || p->transport_size > size_max)
            return -BPAK_SIZE_ERROR;

        uint64_t stored_size = (p->flags & BPAK_FLAG_TRANSPORT) ? p->transport_size
                                                                : bpak_part_decoded_size(p);

        if (stored_size > off_max - stored_end || bpak_part_decoded_size(p) > off_max - decoded_end)
            return -BPAK_SIZE_ERROR;

        stored_end += stored_size;
        decoded_end += bpak_part_decoded_size(p);
    }

    /* The meta data values are aligned and within the meta data array */
    empty = false;

    bpak_foreach_meta(hdr, m) {
        if (!m->id) {
            empty = true;
            continue;
        }

        if (empty)
            return -BPAK_NOT_SUPPORTED;

        if (m->offset % BPAK_META_ALIGN)
            return -BPAK_BAD_ALIGNMENT;

        if ((m->size + m->offset) > BPAK_METADATA_BYTES) {
            return -BPAK_SIZE_ERROR;
        }
    }

    /* Not covered by the header hash */
    if (hdr->signature_sz > sizeof(hdr->signature))
        return -BPAK_SIZE_ERROR;

    if (!hdr->hash_kind)
        return -BPAK_NOT_SUPPORTED;

    return BPAK_OK;
}

BPAK_EXPORT off_t bpak_part_offset(struct bpak_header *h, struct bpak_part_header *part)
{
    off_t offset = sizeof(*h);

    bpak_foreach_used_part(h, p)
    {
        if (p->id == part->id)
            break;

        offset += bpak_part_size(p);
    }

    return offset;
}

off_t bpak_part_data_offset(struct bpak_header *h, struct bpak_part_header *part, off_t data_offset)
{
    return bpak_part_offset(h, part) - (off_t)sizeof(*h) + data_offset;
}

BPAK_EXPORT size_t bpak_part_size(struct bpak_part_header *part)
{
    if (part->flags & BPAK_FLAG_TRANSPORT)
        return part->transport_size;
    else
        return bpak_part_decoded_size(part);
}

BPAK_EXPORT size_t bpak_part_size_wo_pad(struct bpak_part_header *part)
{
    if (part->flags & BPAK_FLAG_TRANSPORT)
        return part->transport_size;
    else
        return (part->size);
}

BPAK_EXPORT const char *bpak_signature_kind(uint8_t signature_kind)
{
    switch (signature_kind) {
    case BPAK_SIGN_PRIME256v1:
        return "prime256v1";
    case BPAK_SIGN_SECP384r1:
        return "secp384r1";
    case BPAK_SIGN_SECP521r1:
        return "secp521r1";
    case BPAK_SIGN_RSA4096:
        return "rsa4096";
    default:
        return "Unknown";
    }
}

BPAK_EXPORT const char *bpak_hash_kind(uint8_t hash_kind)
{
    switch (hash_kind) {
    case BPAK_HASH_SHA256:
        return "sha256";
    case BPAK_HASH_SHA384:
        return "sha384";
    case BPAK_HASH_SHA512:
        return "sha512";
    default:
        return "Unknown";
    }
}

BPAK_EXPORT __attribute__((weak)) int bpak_printf(int verbosity, const char *fmt, ...)
{
    (void)verbosity;
    (void)fmt;

    return BPAK_OK;
}

BPAK_EXPORT int bpak_copyz_signature(struct bpak_header *header, uint8_t *signature, size_t *size)
{
    if (header->signature_sz > BPAK_SIGNATURE_MAX_BYTES)
        return -BPAK_SIZE_ERROR;
    if (header->signature_sz == 0)
        return -BPAK_SIZE_ERROR;
    if (header->signature_sz > *size)
        return -BPAK_SIZE_ERROR;

    (*size) = header->signature_sz;
    memcpy(signature, header->signature, header->signature_sz);
    memset(header->signature, 0, sizeof(header->signature));
    header->signature_sz = 0;

    return BPAK_OK;
}

BPAK_EXPORT int bpak_set_key_id(struct bpak_header *hdr, uint32_t key_id)
{
    hdr->key_id = key_id;
    return BPAK_OK;
}

BPAK_EXPORT int bpak_set_keystore_id(struct bpak_header *hdr, uint32_t keystore_id)
{
    hdr->keystore_id = keystore_id;
    return BPAK_OK;
}

BPAK_EXPORT int bpak_add_transport_meta(struct bpak_header *header,
                                        bpak_id_t part_id,
                                        uint32_t encoder_id,
                                        uint32_t decoder_id)
{
    int rc;
    struct bpak_transport_meta *transport_meta = NULL;
    struct bpak_meta_header *meta = NULL;

    rc = bpak_add_meta(header, BPAK_ID_BPAK_TRANSPORT, part_id, sizeof(*transport_meta), &meta);

    if (rc != BPAK_OK)
        return rc;

    transport_meta = bpak_get_meta_ptr(header, meta, struct bpak_transport_meta);

    transport_meta->alg_id_encode = encoder_id;
    transport_meta->alg_id_decode = decoder_id;

    return BPAK_OK;
}

BPAK_EXPORT const char *bpak_version(void)
{
    return BPAK_VERSION_STRING;
}
