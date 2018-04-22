#include <recovery.h>
#include <board.h>
#include <tinyprintf.h>
#include <io.h>
#include <usb.h>
#include <crc.h>
#include <emmc.h>
#include <plat.h>
#include <gpt.h>
#include <pbimage.h>

#include <plat/imx6ul/caam.h>

void pb_flash_bootloader(u8 *bfr, u32 blocks_to_write) {
    usdhc_emmc_switch_part(1);
    usdhc_emmc_xfer_blocks(2, bfr,blocks_to_write, 1, 0);
    usdhc_emmc_switch_part(2);
    usdhc_emmc_xfer_blocks(2, bfr,blocks_to_write, 1, 0);
    usdhc_emmc_switch_part(0);
}

static void pb_flash_part(u8 part_no, u32 lba_offset, u32 no_of_blocks, u8 *bfr) {
    u32 part_lba_offset = 0;

    // Check for data complete

    part_lba_offset = gpt_get_part_offset(part_no);

    if (!part_lba_offset) {
        tfp_printf ("Unknown partiotion\n\r");
    }
    
    usdhc_emmc_xfer_blocks(part_lba_offset + lba_offset, bfr, no_of_blocks, 1, 1);
}

typedef int func(void);

static void pb_boot_part(u8 part_no) {
    u32 part_lba_offset = 0;
    func* f; // = (func*)0x87800000;
    struct __a4k __no_bss pb_pbi pbi;

    unsigned char sign_copy[1024];
    unsigned int sign_sz = 0;
    unsigned char hash_copy[32];
    unsigned char hash[32];
    int t1, t2;
    part_lba_offset = gpt_get_part_offset(part_no);

    if (!part_lba_offset) {
        tfp_printf ("Unknown partiotion\n\r");
    }

    tfp_printf (" o Reading image header...\n\r");
    usdhc_emmc_xfer_blocks(part_lba_offset, (u8 *) &pbi,
                            sizeof(struct pb_pbi)/512, 0, 0);


    if (pbi.hdr.header_magic != PB_IMAGE_HEADER_MAGIC) {
        tfp_printf ("Incorrect header magic\n\r");
        return;
    }
/*    
    tfp_printf ("%i %i\n\r",sizeof(struct pb_image_hdr),sizeof(struct pb_component_hdr));
    tfp_printf ("Header magic: 0x%8.8X\n\r",pbi.hdr.header_magic);
    tfp_printf (" no_of_components = %i\n\r",pbi.hdr.no_of_components);
    tfp_printf (" SHA256: ");
    for (int i = 0; i < 32; i++)
        tfp_printf("%2.2X",pbi.hdr.sha256[i]);
    tfp_printf("\n\r");

    tfp_printf ("Component 0:\n\r");
    tfp_printf (" header version: %i\n\r",pbi.comp[0].comp_header_version);
    tfp_printf (" size = %i bytes\n\r",pbi.comp[0].component_size);
    tfp_printf (" load addr = 0x%8.8X",pbi.comp[0].load_addr_low);
    tfp_printf (" image offset = 0x%8.8X\n\r",pbi.comp[0].component_offset);
  */  
    f = (func *) pbi.comp[0].load_addr_low;


    tfp_printf (" o Loading component... %i b\n\r",pbi.comp[0].component_size);
    usdhc_emmc_xfer_blocks(part_lba_offset + pbi.comp[0].component_offset/512
                , (u8*) pbi.comp[0].load_addr_low, 
                pbi.comp[0].component_size/512+1, 0, 0);

    if (pbi.hdr.sign_length > sizeof(sign_copy))
            pbi.hdr.sign_length = sizeof(sign_copy);
    
    memcpy(sign_copy, pbi.hdr.sign, pbi.hdr.sign_length);
    sign_sz = pbi.hdr.sign_length;
    pbi.hdr.sign_length = 0;

    memcpy (hash_copy, pbi.hdr.sha256,32);
    
    memset (pbi.hdr.sign, 0, 1024);
    memset (pbi.hdr.sha256, 0, 1024);

    /* CAAM SHA256*/
    struct fsl_caam caam;
    caam.base = 0x02140000;
    caam_init(&caam);
 
    struct caam_hash_ctx ctx;
    caam_sha256_init(&ctx);
    t1 = plat_get_ms_tick();
    caam_sha256_update(&ctx, (const unsigned char*) &pbi.hdr, 
                    sizeof(struct pb_image_hdr));
    caam_sha256_update(&ctx, (const unsigned char*) &pbi.comp[0], 
                    16*sizeof(struct pb_component_hdr));
 
    caam_sha256_update(&ctx, (const unsigned char*) pbi.comp[0].load_addr_low, 
                    pbi.comp[0].component_size);
    caam_sha256_finalize(&caam, &ctx, hash);

    t2 = plat_get_ms_tick();

    u8 flag_chk_ok = 1;

    for (int i = 0; i < 32; i++) {
        if (hash[i] != hash_copy[i])
            flag_chk_ok = 0;
    }
    
    if (flag_chk_ok)
        tfp_printf (" o CHK OK  - ");
    else {
        tfp_printf (" o CHK ERR - ");
   }
    tfp_printf (" SHA256: ");
    for (int i = 0; i < 32; i++)
        tfp_printf("%2.2X",hash[i]);
    tfp_printf(" t = %i ms\n\r",t2-t1);
 


    if (!flag_chk_ok)
        return;

    int err;

    extern u8 _binary____pki_test_rsa_public_der_start;
    extern u8 _binary____pki_test_rsa_public_der_end;

    u8 *pkey_ptr = &_binary____pki_test_rsa_public_der_start;


    u8 __a4k output_data[512];

    t1 = plat_get_ms_tick();
    caam_rsa_enc(&caam,
                    sign_copy, sign_sz,
                    output_data,
                    &pkey_ptr[0x21], 512,      // PK Modulus
                    &pkey_ptr[0x21+512+2], 3); // PK exponent
    t2 = plat_get_ms_tick();
/*
    tfp_printf ("RSA: ");
    for (int i = 0; i < 512; i++) {
        tfp_printf ("%2.2X", output_data[i]);
    }
    tfp_printf(" t = %i ms\n\r");

    tfp_printf ("SIG: ");
    for (int i = 0; i < 32; i++) {
        tfp_printf ("%2.2X",sign_copy[i]);
    }
    tfp_printf(" sz = %i \n\r",sign_sz);
*/

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
        tfp_printf (" o RSA Signature verified, it took %i ms\n\r",t2-t1);

        f();
    } else {
        tfp_printf (" o RSA Signature failed!\n\r");
        return ;
    }

}

void* memcpy(void *dest, const void *src, size_t sz) {
    u8 *p1 = (u8*) dest;
    u8 *p2 = (u8*) src;
    
    for (u32 i = 0; i < sz; i++)
        *p1++ = *p2++;

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    u8 *p1 = (u8*) s1;
    u8 *p2 = (u8*) s2;
    
    for (u32 i = 0; i < n; i++)
        if (*p1++ != *p2++)
            return -1;
    return 0;
}



void recovery_cmd_event(struct pb_usb_cmd *cmd, u8 *bulk_buffer, u8* bulk_buffer2) {
    u32 *no_of_blks = NULL;
    struct pb_usb_cmd resp;
    struct pb_cmd_write_part *wr_part = (struct pb_cmd_write_part*) cmd;
    struct pb_cmd_prep_buffer *prep_bfr = (struct pb_cmd_prep_buffer*) cmd;
    
    switch (cmd->cmd) {
        case PB_CMD_PREP_BULK_BUFFER:
            //tfp_printf("Preparting buffer %i [%i]\n\r",prep_bfr->buffer_id,
            //                                    prep_bfr->no_of_blocks);
            plat_usb_prep_bulk_buffer(prep_bfr->no_of_blocks, prep_bfr->buffer_id);
        break;
        case PB_CMD_FLASH_BOOTLOADER:
            no_of_blks = (u32 *)cmd->data;

            pb_flash_bootloader(bulk_buffer, *no_of_blks);
        break;
        case PB_CMD_GET_VERSION:
            //tfp_printf ("Get version\n\r");
            resp.cmd = PB_CMD_GET_VERSION;
            tfp_sprintf((s8*) resp.data, "PB %s",VERSION);
            plat_usb_send((u8*) &resp, 0x40);
        break;
        case PB_CMD_RESET:
            //tfp_printf ("Reset...\n\r");
            plat_reset();
            while(1);
        break;
        case PB_CMD_GET_GPT_TBL:
            plat_usb_send((u8*) gpt_get_tbl(), sizeof(struct gpt_primary_tbl));
        break;
        case PB_CMD_WRITE_PART:
            //tfp_printf ("Writing %i blks to part %i with offset %8.8X using bfr %i\n\r",
            //            wr_part->no_of_blocks, wr_part->part_no,
            //            wr_part->lba_offset, wr_part->buffer_id);
            if (wr_part->buffer_id) {
                pb_flash_part(wr_part->part_no, wr_part->lba_offset, 
                        wr_part->no_of_blocks, bulk_buffer2);
            } else {
                pb_flash_part(wr_part->part_no, wr_part->lba_offset, 
                        wr_part->no_of_blocks, bulk_buffer);
            }
        break;
        case PB_CMD_BOOT_PART:
            tfp_printf ("\n\rBOOT 1\n\r");
            pb_boot_part(1);
        break;
        default:
            tfp_printf ("Got unknown command: %x\n\r",cmd->cmd);
    }
}


/**
 * -  First measurement ---
 * POR -> entry_armv7a.S ~28ms (First measurement)
 * Writing 250 MByte via USB -> 20MByte /s
 *
 */


void recovery(void) {
    tfp_printf ("\n\r*** RECOVERY MODE ***\n\r");

    pb_writel(0, REG(0x020A0000,0));
    board_usb_init();
    
    u32 loop_count = 0;
    volatile u8 led_blink = 0;

    while(1) {
        loop_count++;

        if (loop_count % 50000 == 0) {
            led_blink = !led_blink;
            pb_writel(led_blink?0x4000:0, REG(0x020A0000,0));

        }
        

        soc_usb_task();
   }



}
