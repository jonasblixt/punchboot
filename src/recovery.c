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
#include <tomcrypt.h>
 

void pb_flash_bootloader(u8 *bfr, u32 blocks_to_write) {
    
    
    tfp_printf ("Blocks: %i\n\r",blocks_to_write);

    tfp_printf ("Flashing BOOT0\n\r");
    usdhc_emmc_switch_part(1);
    tfp_printf ("Switching to BOOT0\n\r");
    usdhc_emmc_xfer_blocks(2, bfr,blocks_to_write, 1, 0);

    tfp_printf ("Flashing BOOT1\n\r");
    usdhc_emmc_switch_part(2);
    tfp_printf ("Switching to BOOT1\n\r");
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
    hash_state md, md2;
	struct ltc_hash_descriptor hash_desc = sha256_desc;
	int hash_idx =  register_hash(&hash_desc);
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

    //tfp_printf ("Calculating SHA256...\n\r");

    t1 = plat_get_ms_tick();
    sha256_init(&md2);
    sha256_process(&md2, (const unsigned char*) &pbi.hdr, 
                    sizeof(struct pb_image_hdr));
    sha256_process(&md2, (const unsigned char*) &pbi.comp[0], 
                    16*sizeof(struct pb_component_hdr));
 
    sha256_process(&md2, (const unsigned char*) pbi.comp[0].load_addr_low, 
                    pbi.comp[0].component_size);
    sha256_done(&md2, hash);
    t2 = plat_get_ms_tick();
    tfp_printf(" t_sha256 = %i ms\n\r",t2-t1);
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
    tfp_printf("\n\r");
    
    if (!flag_chk_ok)
        return;

    int err;

    rsa_key key;
ltc_mp = ltm_desc;
    extern u8 _binary____pki_test_rsa_public_der_start;
    extern u8 _binary____pki_test_rsa_public_der_end;

    u8 *pkey_ptr = &_binary____pki_test_rsa_public_der_start;

    u32 pkey_sz = &_binary____pki_test_rsa_public_der_end -
            &_binary____pki_test_rsa_public_der_start;

    tfp_printf (" o Loading key from 0x%8.8X: ",
                &_binary____pki_test_rsa_public_der_start);
    /*for (int i = 0; i < pkey_sz; i++) 
        tfp_printf ("%2.2X", pkey_ptr[i]);
    tfp_printf ("\n\r");
    */

    t1 = plat_get_ms_tick();
    err = rsa_import (pkey_ptr, pkey_sz,
                            &key);

    t2 = plat_get_ms_tick();
    tfp_printf (" t_rsa_keyimport = %i ms\n\r",t2-t1);

    u32 *key_modulus = (u32 *) key.N;
 
    if (err != CRYPT_OK)  {
        tfp_printf ("ERROR\n\r");
        return;
    } else {
        tfp_printf (" %i bit RSA\n\r",sign_sz*8);
    }

    int stat = 0;
    t1 = plat_get_ms_tick();
    err = rsa_verify_hash_ex (sign_copy, (u32) sign_sz, hash, 32, 
                LTC_PKCS_1_V1_5 , hash_idx, 0, &stat, &key);
    //tfp_printf ("rsa_verify_hash_ex = %i, stat = %i\n\r",err,stat);
    t2 = plat_get_ms_tick();

    tfp_printf ("t_rsa_verify = %i ms\n\r",t2-t1);
    if (err == CRYPT_OK) {
        tfp_printf (" o Signature verified, booting\n\r");
        f();
    } else {
        tfp_printf (" o Invalid image\n\r");
        plat_reset();
    }
}
#define HEAPSIZE 1024*1024*16
unsigned char __a4k __no_bss _heap[HEAPSIZE];

void * malloc ( u32 incr ) {
    static unsigned char *heap_end;
    unsigned char *prev_heap_end;
    
    //tfp_printf ("M called 0x%8.8X\n\r", incr);
    /* initialize */
    if( heap_end == 0 ) heap_end = _heap;

    prev_heap_end = heap_end;

    if( heap_end + incr - _heap > HEAPSIZE ) {
    /* heap overflow - announce on stderr */
    //write( 2, "Heap overflow!\n", 15 );
    //abort();
        tfp_printf ("Heap overflow..\n\r");
        while(1);
    }

    heap_end += incr;
    
    return prev_heap_end;
}

void *calloc(size_t nmemb, size_t size) {
   // tfp_printf ("Warn: calloc called\n\r");
    return malloc (nmemb*size);
}

void free(void *ptr) {
}


void *realloc(void *ptr, size_t size) {
    u8 * new = malloc(size);
    memcpy(new, ptr, size);
    return new;
}

int rand(void) {
    return 0;
}

u8 locale[100];

const char * __locale_ctype_ptr(void) {
    return (u8*)&locale;
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
            tfp_printf("Preparting buffer %i [%i]\n\r",prep_bfr->buffer_id,
                                                prep_bfr->no_of_blocks);
            plat_usb_prep_bulk_buffer(prep_bfr->no_of_blocks, prep_bfr->buffer_id);
        break;
        case PB_CMD_FLASH_BOOTLOADER:
            no_of_blks = (u32 *)cmd->data;

            pb_flash_bootloader(bulk_buffer, *no_of_blks);
        break;
        case PB_CMD_GET_VERSION:
            tfp_printf ("Get version\n\r");
            resp.cmd = PB_CMD_GET_VERSION;
            tfp_sprintf((s8*) resp.data, "PB %s",VERSION);
            plat_usb_send((u8*) &resp, 0x40);
        break;
        case PB_CMD_RESET:
            tfp_printf ("Reset...\n\r");
            plat_reset();
            while(1);
        break;
        case PB_CMD_GET_GPT_TBL:
            plat_usb_send((u8*) gpt_get_tbl(), sizeof(struct gpt_primary_tbl));
        break;
        case PB_CMD_WRITE_PART:
            tfp_printf ("Writing %i blks to part %i with offset %8.8X using bfr %i\n\r",
                        wr_part->no_of_blocks, wr_part->part_no,
                        wr_part->lba_offset, wr_part->buffer_id);
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
