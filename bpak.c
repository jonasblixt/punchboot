/**
 * BPAK - Bit Packer
 *
 * Copyright (C) 2019 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string.h>
#include <bpak/bpak.h>

int bpak_get_meta_and_header(struct bpak_header *hdr, uint32_t id,
        uint32_t part_id_ref, void **ptr, struct bpak_meta_header **header)
{
    void *offset = *ptr;

    bpak_foreach_meta(hdr, m)
    {

        if (m->id == id)
        {
            if (part_id_ref != 0)
            {
                if (m->part_id_ref != part_id_ref)
                    continue;
            }

            void *tmp = (void *) &hdr->metadata[m->offset];

            if (offset)
            {
                if (tmp <= offset)
                    continue;
            }

            (*ptr) = tmp;

            if (header)
                (*header) = m;

            return BPAK_OK;
        }
    }

    return -BPAK_NOT_FOUND;
}


int bpak_get_part(struct bpak_header *hdr, uint32_t id,
                  struct bpak_part_header **part)
{
    void *offset = *part;

    bpak_foreach_part(hdr, p)
    {
        if (p->id == id)
        {
            if (offset)
            {
                if (((void *) p) <= offset)
                    continue;
            }

            (*part) = p;
            return BPAK_OK;
        }
    }

    return -BPAK_NOT_FOUND;
}

int bpak_get_meta(struct bpak_header *hdr, uint32_t id, void **ptr)
{
    return bpak_get_meta_and_header(hdr, id, 0, ptr,
                    (struct bpak_meta_header  **) 0);
}

int bpak_get_meta_with_ref(struct bpak_header *hdr, uint32_t id,
                            uint32_t part_id_ref, void **ptr)
{
    return bpak_get_meta_and_header(hdr, id, part_id_ref, ptr,
                    (struct bpak_meta_header  **) 0);
}

int bpak_valid_header(struct bpak_header *hdr)
{
    if (hdr->magic != BPAK_HEADER_MAGIC)
        return -BPAK_FAILED;

    /* Check alignment of part data blocks */
    bpak_foreach_part(hdr, p)
    {
        if (!p->id)
            break;

        if (((p->size + p->pad_bytes) % BPAK_PART_ALIGN) != 0)
            return -BPAK_BAD_ALIGNMENT;
    }

    /* Check for out-of-bounds metadata */
    bpak_foreach_meta(hdr, m)
    {
        if (!m->id)
            break;

        if ((m->size + m->offset) > BPAK_METADATA_BYTES)
        {
            return -BPAK_SIZE_ERROR;
        }
    }

    if (!hdr->hash_kind)
        return -BPAK_NOT_SUPPORTED;

    return BPAK_OK;
}

uint64_t bpak_part_offset(struct bpak_header *h, struct bpak_part_header *part)
{
    uint64_t offset = sizeof(*h);

    bpak_foreach_part(h, p)
    {
        if (!p->id)
            break;
        if (p->id == part->id)
            break;

        offset += bpak_part_size(p);
    }

    return offset;
}

uint64_t bpak_part_size(struct bpak_part_header *part)
{
    if (part->flags & BPAK_FLAG_TRANSPORT)
        return part->transport_size;
    else
        return (part->size + part->pad_bytes);
}

const char *bpak_known_id(uint32_t id)
{
    switch(id)
    {
    case 0x7da19399:
        return "bpak-key-id";
    case 0x106c13a7:
        return "bpak-key-store";
    case 0xe5679b94:
        return "bpak-signature";
    case 0xfb2f1f3f:
        return "bpak-package";
    case 0x2d44bbfb:
        return "bpak-transport";
    case  0x7c9b2f93:
        return "merkle-salt";
    case 0xe68fc9be:
        return "merkle-root-hash";
    case 0xd1e64a4b:
        return "pb-load-addr";
    case 0x9a5bab69:
        return "bpak-version";
    case 0x0ba87349:
        return "bpak-dependency";
    case 0x9e5e4955:
        return "bpak-key-mask";
    default:
        return "";
    }
}

const char *bpak_signature_kind(uint8_t signature_kind)
{
    switch(signature_kind)
    {
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

const char *bpak_hash_kind(uint8_t hash_kind)
{
    switch(hash_kind)
    {
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

__attribute__ ((weak)) int bpak_printf(int verbosity, const char *fmt, ...)
{
    return BPAK_OK;
}
