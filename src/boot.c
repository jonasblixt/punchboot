/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */



#include <boot.h>
#include <plat.h>
#include <io.h>
#include <gpt.h>
#include <keys.h>
#include <config.h>
#include <pb_string.h>
#include <pb_image.h>
#include <tinyprintf.h>
#include <config.h>

#undef BOOT_DEBUG

static bootfunc *_tee_boot_entry = NULL;
static bootfunc *_boot_entry = NULL;
 
void boot_inc_fail_count(u8 sys_no) {
    /* TODO: Implement me */
}

u32 boot_fail_count(u8 sys_no) {
    /* TODO: Implement me */
    return 0;
}

u32 boot_boot_count(void) {
    u32 cnt = 0;
    config_get_u32(PB_CONFIG_BOOT_COUNT, &cnt);
    return cnt;
}

u32 boot_inc_boot_count(void) {
    u32 cnt = boot_boot_count() + 1;
    config_set_u32(PB_CONFIG_BOOT_COUNT, cnt);
    config_commit();
    return PB_OK;
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
    
#ifdef BOOT_DEBUG
    tfp_printf ("Component manifest:\n\r");
    for (int i = 0; i < pbi.hdr.no_of_components; i++) {
        tfp_printf (" o %i - LA: 0x%8.8X OFF:0x%8.8X \n\r",i, pbi.comp[i].load_addr_low,
                            pbi.comp[i].component_offset);
    }


#endif
    /* TODO: Fix TEE load logic */
    /* TODO: Implement checks to make sure that we don't load stuff
     *    to places we're not allowed to use!
     * */

    for (int i = 0; i < pbi.hdr.no_of_components; i++) {
#ifdef BOOT_DEBUG
        tfp_printf("Loading component %i, %i bytes\n\r",i, pbi.comp[i].component_size);
#endif
        plat_emmc_read_block(part_lba_offset + pbi.comp[i].component_offset/512
                    , (u8*) pbi.comp[i].load_addr_low, 
                    pbi.comp[i].component_size/512+1);
    }

    if (pbi.hdr.sign_length > sizeof(sign_copy))
            pbi.hdr.sign_length = sizeof(sign_copy);
    
    memcpy(sign_copy, pbi.hdr.sign, pbi.hdr.sign_length);
    sign_sz = pbi.hdr.sign_length;
    pbi.hdr.sign_length = 0;

    memcpy (hash_copy, pbi.hdr.sha256,32);
    memset (pbi.hdr.sign, 0, 1024);
    memset (pbi.hdr.sha256, 0, 32);

    plat_sha256_init();

    plat_sha256_update((u8 *) &pbi.hdr, 
                    sizeof(struct pb_image_hdr));

    for (int i = 0; i < pbi.hdr.no_of_components; i++) {
        plat_sha256_update((u8 *) &pbi.comp[i], 
                    sizeof(struct pb_component_hdr));
    }

    for (int i = 0; i < pbi.hdr.no_of_components; i++) {
       plat_sha256_update((u8 *) pbi.comp[i].load_addr_low, 
                        pbi.comp[i].component_size);
    }


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
        _boot_entry =  (bootfunc *) pbi.comp[0].load_addr_low;
        _tee_boot_entry = (bootfunc *) pbi.comp[1].load_addr_low;
        return PB_OK;
    } else {
        _boot_entry = NULL;
        return PB_ERR;
    }
}

void boot(void) {
    volatile u32 val;
 
    if (_tee_boot_entry != NULL) {
#ifdef BOOT_DEBUG
        tfp_printf ("Jumping to TEE 0x%8.8X \n\r", (u32) _tee_boot_entry);
#endif

        asm volatile("mov lr,%1" "\n\r" // When TEE does b LR, the VMM will start
                     "mov pc,%0" "\n\r" // Jump to TEE
                        : 
                        : "r" (_tee_boot_entry), "r" (_boot_entry));


/*
        asm volatile("mrs %0, cpsr" : "=r" (val) :: "cc");
        tfp_printf(" CPSR = 0x%8.8X\n\r", val);
        tfp_printf(" vmm entry = 0x%8.8X\n\r",(u32) _boot_entry);

        _tee_boot_entry();
        tfp_printf("Back from TEE\n\r");
        asm("nop");
        asm("nop");
        asm volatile("mrs %0, cpsr" : "=r" (val) :: "cc");
        tfp_printf(" CPSR = 0x%8.8X\n\r", val);
        tfp_printf(" vmm entry = 0x%8.8X\n\r",(u32) _boot_entry);
 */
        /* TODO: Make sure TEE set HYP mode */
   //     _boot_entry();
   
    } else if (_boot_entry != NULL) {
        _boot_entry();
    }
}


