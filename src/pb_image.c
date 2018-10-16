#include <pb.h>
#include <pb_image.h>
#include <string.h>
#include <tinyprintf.h>
#include <plat.h>
#include <io.h>
#include <gpt.h>
#include <keys.h>

static struct __a4k __no_bss pb_pbi pbi;

uint32_t pb_image_load_from_fs(uint32_t part_lba_offset)
{

    if (!part_lba_offset) {
        LOG_ERR ("Unknown partition\n\r");
        return PB_ERR;
    }

    plat_emmc_read_block(part_lba_offset, (uint8_t *) &pbi,
                            sizeof(struct pb_pbi)/512);


    if (pbi.hdr.header_magic != PB_IMAGE_HEADER_MAGIC) {
        LOG_ERR ("Incorrect header magic\n\r");
        return PB_ERR;
    }
    
    /* TODO: Make sure bootloader code can't be overwritten */

    LOG_INFO ("Component manifest:");
    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) {
        LOG_INFO (" o %lu - LA: 0x%8.8lX OFF:0x%8.8lX",i, pbi.comp[i].load_addr_low,
                            pbi.comp[i].component_offset);
    }

    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) {
        LOG_INFO("Loading component %lu, %lu bytes",i, pbi.comp[i].component_size);
        plat_emmc_read_block(part_lba_offset + pbi.comp[i].component_offset/512
                    , (uint8_t*) pbi.comp[i].load_addr_low, 
                    pbi.comp[i].component_size/512+1);
    }

    return PB_OK;
}

struct pb_pbi * pb_get_image(void)
{
    return &pbi;
}

bool pb_verify_image(struct pb_pbi* pbi, uint32_t key_index)
{
    unsigned char sign_copy[1024];
    unsigned int sign_sz = 0;
    unsigned char hash_copy[32];
    unsigned char hash[32];

    if (pbi->hdr.sign_length > sizeof(sign_copy))
            pbi->hdr.sign_length = sizeof(sign_copy);
    
    memcpy(sign_copy, pbi->hdr.sign, pbi->hdr.sign_length);
    sign_sz = pbi->hdr.sign_length;
    pbi->hdr.sign_length = 0;

    memcpy (hash_copy, pbi->hdr.sha256,32);
    memset (pbi->hdr.sign, 0, 1024);
    memset (pbi->hdr.sha256, 0, 32);

    plat_sha256_init();

    plat_sha256_update((uint8_t *) &pbi->hdr, 
                    sizeof(struct pb_image_hdr));

    for (unsigned int i = 0; i < pbi->hdr.no_of_components; i++) {
        plat_sha256_update((uint8_t *) &pbi->comp[i], 
                    sizeof(struct pb_component_hdr));
    }

    for (unsigned int i = 0; i < pbi->hdr.no_of_components; i++) {
       plat_sha256_update((uint8_t *) pbi->comp[i].load_addr_low, 
                        pbi->comp[i].component_size);
    }


    plat_sha256_finalize(hash);

    uint8_t flag_chk_ok = 1;

    if (memcmp(hash, hash_copy, 32) != 0)
        flag_chk_ok = 0;

    if (!flag_chk_ok) {
        LOG_ERR ("Error: SHA Incorrect");
        return PB_ERR;
    }

    LOG_INFO("SHA OK");
    /* TODO: This needs some more thinkning, reflect HAB state? */
    uint8_t *pkey_ptr = pb_key_get(PB_KEY_DEV);
    uint8_t __a4k output_data[512];

    plat_rsa_enc(sign_copy, sign_sz,
                    output_data,
                    &pkey_ptr[0x21], 512,      // PK Modulus
                    &pkey_ptr[0x21+512+2], 3); // PK exponent

    /* TODO: ASN.1 support functions */
    uint8_t flag_sig_ok = 1;
    int n = 0;
    for (uint32_t i = 512-32; i < 512; i++) {
        if (output_data[i] != hash[n]) {
            flag_sig_ok = 0;
            break;
        }
        n++;
    }
    if (flag_sig_ok)
        LOG_INFO("SIG OK");

    if (flag_sig_ok)
        return PB_OK;
    else
        return PB_ERR;
}