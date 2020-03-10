
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/image.h>
#include <pb/plat.h>
#include <pb/io.h>
#include <pb/timing_report.h>
#include <pb/gpt.h>
#include <pb/crypto.h>
#include <bpak/bpak.h>

extern char _code_start, _code_end,
            _data_region_start, _data_region_end,
            _zero_region_start, _zero_region_end,
            _stack_start, _stack_end, _big_buffer_start, _big_buffer_end;

#define IMAGE_BLK_CHUNK 8192

static __no_bss __a4k uint8_t signature_data[1024];
extern const struct bpak_keystore keystore_pb;

uint32_t pb_image_check_header(struct bpak_header *h)
{
    uint32_t err = PB_OK;


    err = bpak_valid_header(h);

    if (err != BPAK_OK)
    {
        LOG_ERR("Invalid header");
        return PB_ERR;
    }

    bpak_foreach_part(h, p)
    {
        if (!p->id)
            break;


        size_t sz = bpak_part_size(p);
        uint64_t *load_addr = NULL;

                                  /* pb-load-addr */
        err = bpak_get_meta_with_ref(h, 0xd1e64a4b,
                                        p->id, (void **) &load_addr);


        if (err != BPAK_OK)
        {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            break;
        }

        uint64_t la = *load_addr;

        if (PB_CHECK_OVERLAP(la, sz, &_stack_start, &_stack_end))
        {
            err = PB_ERR;
            LOG_ERR("image overlapping with PB stack");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_data_region_start, &_data_region_end))
        {
            err = PB_ERR;
            LOG_ERR("image overlapping with PB data");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_zero_region_start, &_zero_region_end))
        {
            err = PB_ERR;
            LOG_ERR("image overlapping with PB bss");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_code_start, &_code_end))
        {
            err = PB_ERR;
            LOG_ERR("image overlapping with PB code");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_big_buffer_start, &_big_buffer_end))
        {
            err = PB_ERR;
            LOG_ERR("image overlapping with PB buffer");
        }
    }

    return err;
}

uint32_t pb_image_load_from_fs(uint64_t part_lba_start,
                                uint64_t part_lba_end,
                                struct bpak_header *h,
                                const char *hash)
{
    uint32_t err;
    int hash_kind;
    int sign_kind;

    tr_stamp_begin(TR_LOAD);

    if (!part_lba_start)
    {
        LOG_ERR("Unknown partition");
        return PB_ERR;
    }

    err = plat_read_block(part_lba_end - (sizeof(struct bpak_header) / 512) + 1,
                          (uintptr_t) h,
                          (sizeof(struct bpak_header)/512));

    if (err != PB_OK)
    {
        LOG_ERR("Could not read from block device");
        return err;
    }

    err = bpak_valid_header(h);

    if (err != BPAK_OK)
    {
        LOG_ERR("Invalid BPAK header");
        return PB_ERR;
    }

    err = pb_image_check_header(h);

    if (err != PB_OK)
        return err;

    uint32_t *key_id = NULL;
    uint32_t *keystore_id = NULL;

                          /* bpak-key-id */
    err = bpak_get_meta(h, 0x7da19399, (void **) &key_id);

    if (!key_id || (err != BPAK_OK))
    {
        LOG_ERR("Missing bpak-key-id meta\n");
        return PB_ERR;
    }

                        /* bpak-key-store */
    err = bpak_get_meta(h, 0x106c13a7, (void **) &keystore_id);

    if (!keystore_id || (err != BPAK_OK))
    {
        LOG_ERR("Missing bpak-key-store meta\n");
        return PB_ERR;
    }

    LOG_DBG("Key-store: %x", *keystore_id);
    LOG_DBG("Key-ID: %x", *key_id);

    if (*keystore_id != keystore_pb.id)
    {
        LOG_ERR("Invalid key-store");
        return PB_ERR;
    }

    struct bpak_key *k = NULL;

    for (uint32_t i = 0; i < keystore_pb.no_of_keys; i++)
    {
        if (keystore_pb.keys[i]->id == *key_id)
        {
            k = keystore_pb.keys[i];
            break;
        }
    }

    if (!k)
    {
        LOG_ERR("Key not found");
        return PB_ERR;
    }

    switch (h->hash_kind)
    {
        case BPAK_HASH_SHA256:
            hash_kind = PB_HASH_SHA256;
            LOG_DBG("SHA256");
        break;
        case BPAK_HASH_SHA384:
            hash_kind = PB_HASH_SHA384;
        break;
        case BPAK_HASH_SHA512:
            hash_kind = PB_HASH_SHA512;
        break;
        default:
            err = PB_ERR;
    }

    if (err != PB_OK)
        return err;

    switch (h->signature_kind)
    {
        case BPAK_SIGN_PRIME256v1:
            sign_kind = PB_SIGN_PRIME256v1;
        break;
        case BPAK_SIGN_SECP384r1:
            sign_kind = PB_SIGN_SECP384r1;
        break;
        case BPAK_SIGN_SECP521r1:
            sign_kind = PB_SIGN_SECP521r1;
        break;
        case BPAK_SIGN_RSA4096:
            sign_kind = PB_SIGN_RSA4096;
        break;
        default:
            err = PB_ERR;
    }

    if (err != PB_OK)
        return err;

    plat_hash_init(hash_kind);

    bool found_signature = false;

    /* Copy and zero out the signature metadata before hasing header */
    bpak_foreach_meta(h, m)
    {
                    /* bpak-signature */
        if (m->id == 0xe5679b94)
        {
            LOG_DBG("sig zero");
            uint8_t *ptr = &(h->metadata[m->offset]);

            if (m->size > sizeof(signature_data))
            {
                LOG_ERR("Signature metadata is too large\n");
                return PB_ERR;
            }

            memcpy(signature_data, ptr, m->size);
            memset(ptr, 0, m->size);
            memset(m, 0, sizeof(*m));
            found_signature = true;
            break;
        }
    }

    if (!found_signature)
    {
        LOG_ERR("Could not find a valid signature");
        return PB_ERR;
    }

    plat_hash_update((uintptr_t) h, sizeof(struct bpak_header));

    uint64_t *load_addr = NULL;

    bpak_foreach_part(h, p)
    {
        if (!p->id)
            break;

        load_addr = NULL;
                                              /* pb-load-addr */
        err = bpak_get_meta_with_ref(h, 0xd1e64a4b,
                                        p->id, (void **) &load_addr);

        if (err != BPAK_OK)
        {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            break;
        }

        LOG_DBG("Loading part %x --> %p, %llu bytes", p->id,
                        (void *)(uintptr_t) (*load_addr),
                        bpak_part_size(p));


        LOG_DBG("Part offset: %llu", p->offset);
        uint64_t blk_start = part_lba_start + \
                     ((p->offset - sizeof(struct bpak_header)) / 512);

        uint32_t no_of_blks = (uint32_t) (bpak_part_size(p) / 512);

        uint64_t chunk_offset = 0;
        uint32_t c = 0;
        uint8_t *a = (uint8_t *)(uintptr_t) (*load_addr);

        LOG_DBG("no_of_blks = %u", no_of_blks);

        while (no_of_blks)
        {
            uint32_t blk_read = (no_of_blks > IMAGE_BLK_CHUNK)?
                                        IMAGE_BLK_CHUNK:no_of_blks;

            LOG_DBG("R LBA: %llu, to: %p, cnt: %u",
                    blk_start+chunk_offset, (void *) a, blk_read);

            plat_read_block(blk_start+chunk_offset, (uintptr_t) a, blk_read);

            LOG_DBG("Hashing %p, %u bytes", a, blk_read*512);
            plat_hash_update((uintptr_t) a, blk_read*512);
            chunk_offset += blk_read;
            no_of_blks -= blk_read;
            a += blk_read*512;
            c++;
        }


        LOG_DBG("Read %u chunks", c);
    }

    plat_hash_finalize((uintptr_t) NULL, 0, (uintptr_t) hash, PB_HASH_BUF_SZ);

    tr_stamp_end(TR_LOAD);

    err = plat_verify_signature(signature_data, sign_kind,
                                (uint8_t *) hash, hash_kind, k);
    if (err == PB_OK)
        LOG_INFO("Signature Valid");
    else
        LOG_ERR("Signature Invalid");

    return err;
}


uint32_t pb_image_verify(struct bpak_header *h, const char *inhash)
{
    return PB_OK;
}

