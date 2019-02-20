#include <stdio.h>
#include <pb.h>
#include <image.h>
#include <string.h>
#include <plat.h>
#include <io.h>
#include <timing_report.h>
#include <gpt.h>
#include <keys.h>

extern char _code_start, _code_end, 
            _data_region_start, _data_region_end, 
            _zero_region_start, _zero_region_end, 
            _stack_start, _stack_end;

static unsigned char __a4k sign_copy[1024];
static uint8_t __a4k output_data[1024];

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
    }

    return PB_OK;
}

uint32_t pb_image_load_from_fs(uint32_t part_lba_offset, struct pb_pbi *pbi)
{
    uint32_t err;

    tr_stamp_begin(TR_BLOCKREAD);

    if (!part_lba_offset) 
    {
        LOG_ERR ("Unknown partition");
        return PB_ERR;
    }

    plat_read_block(part_lba_offset, (uintptr_t) pbi,
                            (sizeof(struct pb_pbi)/512));

    err = pb_image_check_header(pbi);

    if (err != PB_OK)
        return err;

    for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++) 
    {
        volatile uint32_t a = pbi->comp[i].load_addr_low;

        plat_read_block((part_lba_offset + 
                    (pbi->comp[i].component_offset/512)),
                    (uintptr_t) a, 
                    ((pbi->comp[i].component_size/512)+1));

    }

    tr_stamp_end(TR_BLOCKREAD);
    return PB_OK;
}


bool pb_image_verify(struct pb_pbi* pbi)
{
    unsigned int sign_sz = 0;
    unsigned char hash_copy[32];
    unsigned char hash[32];
    bool key_is_revoked = true;
    uint32_t err = PB_OK;

    if (pbi->hdr.sign_length > sizeof(sign_copy))
            pbi->hdr.sign_length = sizeof(sign_copy);
    
    memcpy(sign_copy, pbi->hdr.sign, pbi->hdr.sign_length);
    sign_sz = pbi->hdr.sign_length;
    pbi->hdr.sign_length = 0;

    tr_stamp_begin(TR_SHA);

    memcpy (hash_copy, pbi->hdr.sha256,32);
    memset (pbi->hdr.sign, 0, 1024);
    memset (pbi->hdr.sha256, 0, 32);

    err = pb_is_key_revoked(pbi->hdr.key_index, &key_is_revoked);

    if (err != PB_OK)
        return err;

    if (key_is_revoked)
    {
        LOG_ERR("Key is revoked");
        return PB_KEY_REVOKED_ERROR;
    }
    plat_sha256_init();

    plat_sha256_update((uintptr_t)&pbi->hdr, 
                    sizeof(struct pb_image_hdr));

    for (unsigned int i = 0; i < pbi->hdr.no_of_components; i++) 
    {
        plat_sha256_update((uintptr_t) &pbi->comp[i], 
                    sizeof(struct pb_component_hdr));
    }

    for (unsigned int i = 0; i < pbi->hdr.no_of_components; i++) 
    {
        plat_sha256_update((uintptr_t) pbi->comp[i].load_addr_low, 
                        pbi->comp[i].component_size);
    }

    plat_sha256_finalize((uintptr_t) hash);

    uint8_t flag_chk_ok = 1;

    if (memcmp(hash, hash_copy, 32) != 0)
        flag_chk_ok = 0;

    if (!flag_chk_ok) 
    {
        LOG_ERR ("Error: SHA Incorrect");
        return PB_ERR;
    }

    LOG_INFO("SHA OK");
    tr_stamp_end(TR_SHA);

    tr_stamp_begin(TR_RSA);
    memset(output_data, 0, 512);

    struct asn1_key *k = pb_key_get(pbi->hdr.key_index);

    LOG_INFO("Key index %u", pbi->hdr.key_index);

    if (k == NULL)
    {
        LOG_ERR("Invalid key");
        return PB_ERR;
    }

    err = plat_rsa_enc(sign_copy, sign_sz,
                    output_data, k);

    if (err != PB_OK)
    {
        LOG_ERR("plat_rsa_enc failed");
        return err;
    }

    /* Output is ASN.1 coded, this extracts the decoded checksum */
    /* TODO: Add some ASN1 helpers */
    uint8_t flag_sig_ok = 1;
    int n = 0;
    for (uint32_t i = (512-32); i < 512; i++) 
    {
        if (output_data[i] != hash[n]) 
        {
            flag_sig_ok = 0;
        }
        n++;
    }

    if (flag_sig_ok)
    {
        LOG_INFO("SIG OK");

        LOG_INFO("Key revoke mask: %08x", pbi->hdr.key_revoke_mask);

        pb_update_key_revoke_mask(pbi->hdr.key_revoke_mask);
    }

    tr_stamp_end(TR_RSA);


    if (flag_sig_ok)
        return PB_OK;
    else
        return PB_ERR;
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

