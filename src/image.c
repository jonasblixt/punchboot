#include <stdio.h>
#include <pb.h>
#include <image.h>
#include <string.h>
#include <plat.h>
#include <io.h>
#include <timing_report.h>
#include <gpt.h>
#include <crypto.h>

extern char _code_start, _code_end, 
            _data_region_start, _data_region_end, 
            _zero_region_start, _zero_region_end, 
            _stack_start, _stack_end, _big_buffer_start, _big_buffer_end;

#define IMAGE_BLK_CHUNK 8192

static unsigned char hash_data[64];

uint32_t pb_image_check_header(struct pb_pbi *pbi)
{

    if (pbi->hdr.header_magic != PB_IMAGE_HEADER_MAGIC) 
    {
        LOG_ERR ("Incorrect header magic");
        return PB_ERR;
    }

    for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++) 
    {
        LOG_INFO("Checking component %u, %u bytes",i, 
                                pbi->comp[i].component_size);

        uintptr_t la = (uintptr_t) pbi->comp[i].load_addr_low;
        uint32_t sz = pbi->comp[i].component_size;


        if (PB_CHECK_OVERLAP(la,sz,&_stack_start,&_stack_end))
        {
            LOG_ERR("image overlapping with PB stack");
            return PB_ERR;
        }

        if (PB_CHECK_OVERLAP(la,sz,&_data_region_start,&_data_region_end))
        {
            LOG_ERR("image overlapping with PB data");
            return PB_ERR;
        }

        if (PB_CHECK_OVERLAP(la,sz,&_zero_region_start,&_zero_region_end))
        {
            LOG_ERR("image overlapping with PB bss");
            return PB_ERR;
        }

        if (PB_CHECK_OVERLAP(la,sz,&_code_start,&_code_end))
        {
            LOG_ERR("image overlapping with PB code");
            return PB_ERR;
        }

        if (PB_CHECK_OVERLAP(la,sz,&_big_buffer_start,&_big_buffer_end))
        {
            LOG_ERR("image overlapping with PB buffer");
            return PB_ERR;
        }
    }

    return PB_OK;
}

uint32_t pb_image_load_from_fs(uint32_t part_lba_offset, struct pb_pbi *pbi,
                                const char *hash)
{
    uint32_t err;

    tr_stamp_begin(TR_LOAD);

    if (!part_lba_offset) 
    {
        LOG_ERR ("Unknown partition");
        return PB_ERR;
    }

    LOG_DBG( "LBA: %lu, pbi %p", part_lba_offset, pbi);

    err = plat_read_block(part_lba_offset, (uintptr_t) pbi,
                            (sizeof(struct pb_pbi)/512));

    if (err != PB_OK)
        return err;

    err = pb_image_check_header(pbi);

    if (err != PB_OK)
        return err;

    plat_hash_init(pbi->hdr.hash_kind);

    plat_hash_update((uintptr_t)&pbi->hdr, 
                    sizeof(struct pb_image_hdr));

    for (unsigned int i = 0; i < pbi->hdr.no_of_components; i++) 
    {
        plat_hash_update((uintptr_t) &pbi->comp[i], 
                    sizeof(struct pb_component_hdr));
    }

    for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++) 
    {
        volatile uint32_t a = pbi->comp[i].load_addr_low;
        uint32_t blk_start = part_lba_offset + 
                                    (pbi->comp[i].component_offset/512);
        uint32_t no_of_blks = (pbi->comp[i].component_size/512);
        uint32_t chunk_offset = 0;
        uint32_t c = 0;

        while (no_of_blks)
        {
            uint32_t blk_read = (no_of_blks>IMAGE_BLK_CHUNK)?
                                        IMAGE_BLK_CHUNK:no_of_blks;
            
            LOG_DBG("R LBA: %u, to: %08x, cnt: %u",
                    blk_start+chunk_offset, a, blk_read);

            plat_read_block(blk_start+chunk_offset, (uintptr_t) a, blk_read);
            plat_hash_update((uintptr_t) a, (blk_read*512));
            chunk_offset += blk_read;
            no_of_blks -= blk_read;
            a += blk_read*512;
            c++;
            LOG_DBG("no_of_blks = %u",no_of_blks);
        }

        LOG_DBG("Read %u chunks", c);
    }

    plat_hash_finalize((uintptr_t) hash);
    tr_stamp_end(TR_LOAD);
    return PB_OK;
}


uint32_t pb_image_verify(struct pb_pbi* pbi, const char *inhash)
{
    unsigned char *hash;
    uint32_t err = PB_OK;

    tr_stamp_begin(TR_VERIFY);

    /* No supplied hash, calculate it */
    if (inhash == NULL)
    {
        hash = hash_data;
        plat_hash_init(pbi->hdr.hash_kind);
        plat_hash_update((uintptr_t)&pbi->hdr, 
                        sizeof(struct pb_image_hdr));

        for (unsigned int i = 0; i < pbi->hdr.no_of_components; i++) 
        {
            plat_hash_update((uintptr_t) &pbi->comp[i], 
                        sizeof(struct pb_component_hdr));
        }

        for (unsigned int i = 0; i < pbi->hdr.no_of_components; i++) 
        {
            plat_hash_update((uintptr_t) pbi->comp[i].load_addr_low, 
                            pbi->comp[i].component_size);
        }

        plat_hash_finalize((uintptr_t) hash);
    }
    else
    {
        hash = (unsigned char *) inhash;
    }

    struct pb_key *k;

    LOG_DBG("Loading key %u", pbi->hdr.key_index);
    err = pb_crypto_get_key(pbi->hdr.key_index, &k);

    if (err != PB_OK)
    {
        LOG_ERR("Could not read key");
        return PB_ERR;
    }

    err = plat_verify_signature(pbi->sign, pbi->hdr.sign_kind,
                                hash, pbi->hdr.hash_kind,
                                k);

    if (err != PB_OK)
        return err;

    tr_stamp_end(TR_VERIFY);
    return PB_OK;
}

struct pb_component_hdr * pb_image_get_component(struct pb_pbi *pbi, 
                                            uint32_t comp_type)
{
    if (pbi == NULL)
        return NULL;

    for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++)
    {
        if (pbi->comp[i].component_type == comp_type)
            return &pbi->comp[i];
    }

    return NULL;
}

