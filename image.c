
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


static int pb_image_check_header(struct bpak_header *h)
{
    int err = PB_OK;

    err = bpak_valid_header(h);

    if (err != BPAK_OK)
    {
        LOG_ERR("Invalid header");
        return -PB_ERR;
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
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB stack");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_data_region_start, &_data_region_end))
        {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB data");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_zero_region_start, &_zero_region_end))
        {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB bss");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_code_start, &_code_end))
        {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB code");
        }

        if (PB_CHECK_OVERLAP(la, sz, &_big_buffer_start, &_big_buffer_end))
        {
            err = -PB_ERR;
            LOG_ERR("image overlapping with PB buffer");
        }
    }

    return err;
}

int pb_image_load(struct pb_image_load_context *ctx,
                  struct pb_crypto *crypto,
                  struct bpak_keystore *keystore)
{
    int rc;
    struct pb_hash_context hash;
    struct bpak_header *h = &ctx->header;
    struct bpak_key *k = NULL;
    int hash_kind;
    uint32_t *key_id = NULL;
    uint32_t *keystore_id = NULL;

    tr_stamp_begin(TR_LOAD);

    rc = bpak_valid_header(h);

    if (rc != BPAK_OK)
    {
        LOG_ERR("Invalid BPAK header");
        return PB_ERR;
    }

    rc = pb_image_check_header(h);

    if (rc != PB_OK)
        return rc;

                          /* bpak-key-id */
    rc = bpak_get_meta(h, 0x7da19399, (void **) &key_id);

    if (!key_id || (rc != BPAK_OK))
    {
        LOG_ERR("Missing bpak-key-id meta\n");
        return -PB_ERR;
    }

                        /* bpak-key-store */
    rc = bpak_get_meta(h, 0x106c13a7, (void **) &keystore_id);

    if (!keystore_id || (rc != BPAK_OK))
    {
        LOG_ERR("Missing bpak-key-store meta\n");
        return -PB_ERR;
    }

    LOG_DBG("Key-store: %x", *keystore_id);
    LOG_DBG("Key-ID: %x", *key_id);

    if (*keystore_id != keystore->id)
    {
        LOG_ERR("Invalid key-store");
        return -PB_ERR;
    }

    for (int i = 0; i < keystore->no_of_keys; i++)
    {
        if (keystore->keys[i]->id == *key_id)
        {
            k = keystore->keys[i];
            break;
        }
    }

    if (!k)
    {
        LOG_ERR("Key not found");
        return -PB_ERR;
    }

    switch (h->hash_kind)
    {
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
            rc = -PB_ERR;
    }

    if (rc != PB_OK)
        return rc;

    rc = pb_hash_init(crypto, &hash, hash_kind);

    if (rc != PB_OK)
        return rc;

    ctx->signature_sz = 0;

    /* Copy and zero out the signature metadata before hasing header */
    bpak_foreach_meta(h, m)
    {
                    /* bpak-signature */
        if (m->id == 0xe5679b94)
        {
            LOG_DBG("sig zero");
            uint8_t *ptr = &(h->metadata[m->offset]);

            if (m->size > sizeof(ctx->signature))
            {
                LOG_ERR("Signature metadata is too large\n");
                return -PB_ERR;
            }

            memcpy(ctx->signature, ptr, m->size);
            ctx->signature_sz = m->size;
            memset(ptr, 0, m->size);
            memset(m, 0, sizeof(*m));
            break;
        }
    }

    if (!ctx->signature_sz)
    {
        LOG_ERR("Could not find a valid signature");
        return -PB_ERR;
    }

    rc = pb_hash_update(&hash, h, sizeof(*h));

    if (rc != PB_OK)
        return rc;

    uint64_t *load_addr = NULL;

    bpak_foreach_part(h, p)
    {
        if (!p->id)
            break;

        load_addr = NULL;
                                   /* pb-load-addr */
        rc = bpak_get_meta_with_ref(h, 0xd1e64a4b, p->id, (void **) &load_addr);

        if (rc != BPAK_OK)
        {
            LOG_ERR("Could not read pb-entry for part %x", p->id);
            break;
        }


        LOG_DBG("Loading part %x --> %p, %llu bytes", p->id,
                                (void *)(uintptr_t) (*load_addr),
                                bpak_part_size(p));
        LOG_DBG("Part offset: %llu", p->offset);

        size_t bytes_to_read = bpak_part_size(p);
        size_t chunk_size = 0;
        size_t offset = 0;

        while (bytes_to_read)
        {
            chunk_size = bytes_to_read>ctx->chunk_size? \
                                    ctx->chunk_size:bytes_to_read;

            uintptr_t addr = ((*load_addr) + offset);

            rc = ctx->read(ctx, (void *) addr, chunk_size);

            if (rc != PB_OK)
                break;

            rc = pb_hash_update(&hash, (void *) addr, chunk_size);

            if (rc != PB_OK)
                break;

            bytes_to_read -= chunk_size;
            offset += chunk_size;
        }

        if (ctx->result)
        {
            rc = ctx->result(ctx, rc);

            if (rc != PB_OK)
                return rc;
        }
    }

    rc = pb_hash_finalize(&hash, NULL, 0);

    if (rc != PB_OK)
        return rc;

    tr_stamp_end(TR_LOAD);

    rc = pb_pk_verify(crypto, ctx->signature, ctx->signature_sz, &hash ,k);

    if (rc == PB_OK)
        LOG_INFO("Signature Valid");
    else
        LOG_ERR("Signature Invalid");

    return rc;
}
