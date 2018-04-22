#include <boot.h>
#include <plat.h>
#include <gpt.h>
#include <keys.h>
#include <config.h>
#include <pb_string.h>
#include <pb_image.h>
#include <tinyprintf.h>

static bootfunc* _tee_boot_entry = NULL;
static bootfunc* _boot_entry = NULL;
 
void boot_inc_fail_count(u8 sys_no) {
    /* TODO: Implement me */
}

u32 boot_fail_count(u8 sys_no) {
    /* TODO: Implement me */
    return 0;
}

u32 boot_boot_count(u8 sys_no) {
    /* TODO: Implement me */
    return 0;
}

u32 boot_load(u8 sys_no) {
    u32 part_lba_offset = 0;
    struct __a4k __no_bss pb_pbi pbi;
    unsigned char sign_copy[1024];
    unsigned int sign_sz = 0;
    unsigned char hash_copy[32];
    unsigned char hash[32];

    _boot_entry = NULL;
    _tee_boot_entry = NULL;

    /* TODO: Implement logic to resolve System A UUID -> part_no*/

    part_lba_offset = gpt_get_part_offset(1);

    if (!part_lba_offset) {
        tfp_printf ("Unknown partiotion\n\r");
    }

    plat_emmc_read_block(part_lba_offset, (u8 *) &pbi,
                            sizeof(struct pb_pbi)/512);


    if (pbi.hdr.header_magic != PB_IMAGE_HEADER_MAGIC) {
        tfp_printf ("Incorrect header magic\n\r");
        return PB_ERR;
    }
    
    /* TODO: Fix TEE load logic */

    plat_emmc_read_block(part_lba_offset + pbi.comp[0].component_offset/512
                , (u8*) pbi.comp[0].load_addr_low, 
                pbi.comp[0].component_size/512+1);

    if (pbi.hdr.sign_length > sizeof(sign_copy))
            pbi.hdr.sign_length = sizeof(sign_copy);
    
    memcpy(sign_copy, pbi.hdr.sign, pbi.hdr.sign_length);
    sign_sz = pbi.hdr.sign_length;
    pbi.hdr.sign_length = 0;

    memcpy (hash_copy, pbi.hdr.sha256,32);
    memset (pbi.hdr.sign, 0, 1024);
    memset (pbi.hdr.sha256, 0, 1024);

    plat_sha256_init();

    plat_sha256_update((u8 *) &pbi.hdr, 
                    sizeof(struct pb_image_hdr));
    plat_sha256_update((u8 *) &pbi.comp[0], 
                    16*sizeof(struct pb_component_hdr));
 
    plat_sha256_update((u8 *) pbi.comp[0].load_addr_low, 
                    pbi.comp[0].component_size);
    plat_sha256_finalize(hash);

    u8 flag_chk_ok = 1;

    if (memcmp(hash, hash_copy, 32) != 0)
        flag_chk_ok = 0;

    if (!flag_chk_ok) {
        tfp_printf ("Error: SHA Incorrect\n\r");
        return PB_ERR;
    }

    /* TODO: This needs some more thinkning, reflect HAB state? */
    u8 *pkey_ptr = pb_key_get(PB_KEY_DEV);
    u8 __a4k output_data[512];

    plat_rsa_enc(sign_copy, sign_sz,
                    output_data,
                    &pkey_ptr[0x21], 512,      // PK Modulus
                    &pkey_ptr[0x21+512+2], 3); // PK exponent

    /* TODO: ASN.1 support functions */
    u8 flag_sig_ok = 1;
    int n = 0;
    for (int i = 512-32; i < 512; i++) {
        if (output_data[i] != hash[n]) {
            flag_sig_ok = 0;
            break;
        }
        n++;
    }

   
    if (flag_sig_ok) {
        _boot_entry = (bootfunc *) pbi.comp[0].load_addr_low;
        return PB_OK;
    } else {
        _boot_entry = NULL;
        return PB_ERR;
    }
}

void boot(void) {
    if (_boot_entry != NULL)
        _boot_entry();
}


