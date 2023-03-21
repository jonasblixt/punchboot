/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <string.h>
#include <inttypes.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/plat.h>
#include <bpak/id.h>
#include <boot/image_helpers.h>

extern struct bpak_keystore keystore_pb;
static uint8_t signature[512];

int boot_image_auth_header(struct bpak_header *hdr)
{
    int rc;
    bool key_active = false;
    int hash_kind;
    struct bpak_key *k = NULL;
    size_t signature_length;
    uint8_t header_digest[64];

    rc = bpak_valid_header(hdr);

    if (rc != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    LOG_DBG("Key-store: %x", hdr->keystore_id);
    LOG_DBG("Key-ID: %x", hdr->key_id);

    if (hdr->keystore_id != keystore_pb.id) {
        return -PB_ERR_BAD_KEYSTORE;
    }

    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id == hdr->key_id) {
            k = keystore_pb.keys[i];
            break;
        }
    }

    if (!k) {
        return -PB_ERR_KEY_NOT_FOUND;
    }

    rc = plat_slc_key_active(hdr->key_id, &key_active);

    if (rc != PB_OK) {
        return rc;
    }

    if (!key_active) {
        return -PB_ERR_KEY_REVOKED;
    }

    switch (hdr->hash_kind) {
        case BPAK_HASH_SHA256:
            hash_kind = PB_HASH_SHA256;
        break;
        case BPAK_HASH_SHA384:
            hash_kind = PB_HASH_SHA384;
        break;
        case BPAK_HASH_SHA512:
            hash_kind = PB_HASH_SHA512;
        break;
        default:
            return -PB_ERR_UNKNOWN_HASH;
    }

    rc = plat_hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    signature_length = sizeof(signature);

    rc = bpak_copyz_signature(hdr, signature, &signature_length);

    if (rc != BPAK_OK)
        return -PB_ERR_MEM;

    rc = plat_hash_update((uint8_t *) hdr, sizeof(*hdr));

    if (rc != PB_OK)
        return rc;

    rc = plat_hash_output(header_digest, sizeof(header_digest));

    if (rc != PB_OK)
        return rc;

    rc = plat_pk_verify(signature,
                        signature_length,
                        header_digest,
                        hash_kind,
                        k);

    if (rc == PB_OK) {
        LOG_INFO("Signature valid");
    } else {
        LOG_ERR("Signature invalid");
        return rc;
    }

    return rc;
}

int boot_image_load_and_hash(struct bpak_header *hdr,
                             size_t load_chunk_size,
                             boot_read_cb_t read_f,
                             boot_result_cb_t result_f,
                             uint8_t *payload_digest,
                             size_t payload_digest_size)
{
    int rc;
    uintptr_t load_addr;
    int hash_kind;
    size_t hash_length;
    struct bpak_meta_header *mh;

    rc = bpak_valid_header(hdr);

    if (rc != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    switch (hdr->hash_kind) {
        case BPAK_HASH_SHA256:
            hash_kind = PB_HASH_SHA256;
            hash_length = 32;
        break;
        case BPAK_HASH_SHA384:
            hash_kind = PB_HASH_SHA384;
            hash_length = 48;
        break;
        case BPAK_HASH_SHA512:
            hash_kind = PB_HASH_SHA512;
            hash_length = 64;
        break;
        default:
            return -PB_ERR_UNKNOWN_HASH;
    }

    if (payload_digest_size < hash_length)
        return -PB_ERR_PARAM;

    rc = plat_hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        load_addr = 0;
        rc = bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, p->id, &mh);

        if (rc != BPAK_OK) {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            rc = -PB_ERR_BAD_META;
            break;
        }

        load_addr = (uintptr_t) *bpak_get_meta_ptr(hdr, mh, uint64_t);

        LOG_DBG("Loading part %x --> %"PRIxPTR", %zu bytes",
                 p->id, load_addr, bpak_part_size(p));

        size_t bytes_to_read = bpak_part_size(p);
        size_t chunk_size = 0;
        size_t offset = 0;

        while (bytes_to_read) {
            chunk_size = (bytes_to_read>load_chunk_size)? \
                            load_chunk_size:bytes_to_read;

            uintptr_t addr = load_addr + offset;

            if (read_f) {
                size_t read_lba = (offset +
                                   bpak_part_offset(hdr, p) -
                                   sizeof(struct bpak_header)) / 512;

                rc = read_f(read_lba, chunk_size, addr);

                if (rc != PB_OK)
                    break;
            }

            rc = plat_hash_update((uint8_t *) addr, chunk_size);

            if (rc != PB_OK)
                break;

            bytes_to_read -= chunk_size;
            offset += chunk_size;
        }

        if (result_f) {
            rc = result_f(rc);
        }

        if (rc != PB_OK)
            return rc;
    }

    return plat_hash_output(payload_digest, payload_digest_size);
}

int boot_image_verify_payload(struct bpak_header *hdr,
                              uint8_t *payload_digest)
{
    int rc;
    size_t digest_length;

    rc = bpak_valid_header(hdr);

    if (rc != BPAK_OK) {
        return -PB_ERR_BAD_HEADER;
    }

    switch (hdr->hash_kind) {
        case BPAK_HASH_SHA256:
            digest_length = 32;
        break;
        case BPAK_HASH_SHA384:
            digest_length = 48;
        break;
        case BPAK_HASH_SHA512:
            digest_length = 64;
        break;
        default:
            return -PB_ERR_UNKNOWN_HASH;
    }

    if (memcmp(hdr->payload_hash, payload_digest, digest_length) != 0) {
        return -PB_ERR_BAD_PAYLOAD;
    }

    return PB_OK;
}
