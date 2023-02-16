
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
#include <pb/time.h>
#include <pb/gpt.h>
#include <pb/crypto.h>
#include <bpak/bpak.h>

extern char _code_start, _code_end,
            _data_region_start, _data_region_end,
            _zero_region_start, _zero_region_end,
            _stack_start, _stack_end, _big_buffer_start, _big_buffer_end;

extern struct bpak_keystore keystore_pb;

static struct bpak_header header __a4k __no_bss;
static uint8_t signature[512] __a4k __no_bss;
static size_t signature_sz;
static struct pb_hash_context hash;
static struct pb_timestamp ts_load = TIMESTAMP("Image load and hash");
static struct pb_timestamp ts_signature = TIMESTAMP("Verify signature");

int pb_image_check_header(void)
{
    int err = PB_OK;
    struct bpak_header *h = &header;
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


struct bpak_header *pb_image_header(void)
{
    return &header;
}

int pb_image_load(pb_image_read_t read_f,
                  pb_image_result_t result_f,
                  size_t load_chunk_size,
                  void *priv)
{
    int rc;
    struct bpak_header *h = &header;
    struct bpak_key *k = NULL;
    int hash_kind;

    timestamp_begin(&ts_load);

    rc = bpak_valid_header(h);

    if (rc != BPAK_OK)
    {
        LOG_ERR("Invalid BPAK header");
        return PB_ERR;
    }

    LOG_DBG("Key-store: %x", h->keystore_id);
    LOG_DBG("Key-ID: %x", h->key_id);

    if (h->keystore_id != keystore_pb.id)
    {
        LOG_ERR("Invalid key-store");
        return -PB_ERR;
    }

    for (int i = 0; i < keystore_pb.no_of_keys; i++)
    {
        if (keystore_pb.keys[i]->id == h->key_id)
        {
            k = keystore_pb.keys[i];
            break;
        }
    }

    if (!k)
    {
        LOG_ERR("Key not found");
        return -PB_ERR;
    }

    bool active = false;

    rc = plat_slc_key_active(h->key_id, &active);

    if (rc != PB_OK)
        return rc;

    if (!active)
    {
        LOG_ERR("Invalid or revoked key (%x)", h->key_id);
        return -PB_ERR;
    }

    size_t hash_sz = 0;

    switch (h->hash_kind)
    {
        case BPAK_HASH_SHA256:
            hash_kind = PB_HASH_SHA256;
            hash_sz = 32;
        break;
        case BPAK_HASH_SHA384:
            hash_kind = PB_HASH_SHA384;
            hash_sz = 48;
        break;
        case BPAK_HASH_SHA512:
            hash_kind = PB_HASH_SHA512;
            hash_sz = 64;
        break;
        default:
            LOG_ERR("Unknown hash_kind value 0x%x", h->hash_kind);
            rc = -PB_ERR;
    }

    if (rc != PB_OK)
        return rc;

    rc = plat_hash_init(&hash, hash_kind);

    if (rc != PB_OK)
        return rc;

    signature_sz = sizeof(signature);

    rc = bpak_copyz_signature(h, signature, &signature_sz);

    if (rc != PB_OK) {
        LOG_ERR("Invalid signature area: size=%d", h->signature_sz);
        return rc;
    }

    rc = plat_hash_update(&hash, h, sizeof(*h));

    if (rc != PB_OK)
        return rc;

    rc = plat_hash_finalize(&hash, NULL, 0);

    if (rc != PB_OK)
        return rc;

    timestamp_begin(&ts_signature);

    rc = plat_pk_verify(signature, signature_sz, &hash, k);

    if (rc == PB_OK)
    {
        LOG_INFO("Signature Valid");
    }
    else
    {
        LOG_ERR("Signature Invalid");
        return rc;
    }

    timestamp_end(&ts_signature);

    rc = pb_image_check_header();

    if (rc != PB_OK)
        return rc;

    if (result_f)
    {
        rc = result_f(rc, priv);

        if (rc != PB_OK)
            return rc;
    }

    /* Compute payload hash */

    rc = plat_hash_init(&hash, hash_kind);

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
            chunk_size = (bytes_to_read>load_chunk_size)? \
                            load_chunk_size:bytes_to_read;

            uintptr_t addr = ((*load_addr) + offset);

            rc = read_f((void *) addr, chunk_size, priv);

            if (rc != PB_OK)
                break;

            rc = plat_hash_update(&hash, (void *) addr, chunk_size);

            if (rc != PB_OK)
                break;

            bytes_to_read -= chunk_size;
            offset += chunk_size;
        }

        if (result_f)
        {
            rc = result_f(rc, priv);

            if (rc != PB_OK)
                return rc;
        }
    }

    if (rc != PB_OK)
    {
        LOG_ERR("Loading failed");
        return rc;
    }

    rc = plat_hash_finalize(&hash, NULL, 0);

    if (rc != PB_OK)
        return rc;

    if (memcmp(h->payload_hash, hash.buf, hash_sz) != 0)
    {
        LOG_ERR("Payload hash incorrect");
        return -1;
    }

    timestamp_end(&ts_load);

    return rc;
}
