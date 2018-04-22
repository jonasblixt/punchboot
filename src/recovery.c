#include <recovery.h>
#include <pb_image.h>
#include <plat.h>
#include <keys.h>
#include <io.h>
#include <board.h>
#include <tinyprintf.h>
#include <gpt.h>
#include <boot.h>
#include <config.h>

static void recovery_flash_bootloader(u8 *bfr, u32 blocks_to_write) {
    if (plat_emmc_switch_part(PLAT_EMMC_PART_BOOT0) != PB_OK) {
        tfp_printf ("Error: Could not switch partition\n\r");
    }
    plat_emmc_switch_part(PLAT_EMMC_PART_BOOT0);
    plat_emmc_write_block(2, bfr, blocks_to_write);

    plat_emmc_switch_part(PLAT_EMMC_PART_BOOT1);
    plat_emmc_write_block(2, bfr, blocks_to_write);

    plat_emmc_switch_part(PLAT_EMMC_PART_USER);
 
}

static void recovery_flash_part(u8 part_no, u32 lba_offset, u32 no_of_blocks, u8 *bfr) {
    u32 part_lba_offset = 0;

    // Check for data complete

    part_lba_offset = gpt_get_part_offset(part_no);

    if (!part_lba_offset) {
        tfp_printf ("Unknown partiotion\n\r");
    }
    
    plat_emmc_write_block(part_lba_offset + lba_offset, bfr, no_of_blocks);
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


/* TODO: This needs a total make-over */
void recovery_cmd_event(u8 *cmd_buf, u8 *bulk_buffer, u8* bulk_buffer2) {
    u32 *no_of_blks = NULL;
    u32 *cmd = (u32 *) cmd_buf;
    u32 *cmd_data = (u32 *) cmd_buf + 1;

    struct pb_usb_cmd resp;
    struct pb_cmd_write_part *wr_part = (struct pb_cmd_write_part*) cmd_buf;
    struct pb_cmd_prep_buffer *prep_bfr = (struct pb_cmd_prep_buffer*) cmd_buf;
    
    switch (*cmd) {
        case PB_CMD_PREP_BULK_BUFFER:
            tfp_printf("Preparting buffer %i [%i]\n\r",prep_bfr->buffer_id,
                                                prep_bfr->no_of_blocks);
            plat_usb_prep_bulk_buffer(prep_bfr->no_of_blocks, prep_bfr->buffer_id);
        break;
        case PB_CMD_FLASH_BOOTLOADER:
            no_of_blks = (u32 *)cmd_data;
            tfp_printf ("Flash BL %i\n\r",*no_of_blks);
            recovery_flash_bootloader(bulk_buffer, *no_of_blks);
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
                recovery_flash_part(wr_part->part_no, wr_part->lba_offset, 
                        wr_part->no_of_blocks, bulk_buffer2);
            } else {
                recovery_flash_part(wr_part->part_no, wr_part->lba_offset, 
                        wr_part->no_of_blocks, bulk_buffer);
            }
        break;
        case PB_CMD_BOOT_PART:
            tfp_printf ("\n\rBOOT 1\n\r");
            if (boot_load(1) == PB_OK)
                boot();
        break;
        default:
            tfp_printf ("Got unknown command: %x\n\r",*cmd);
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
    plat_usb_cmd_callback(&recovery_cmd_event);

    u32 loop_count = 0;
    volatile u8 led_blink = 0;

    while(1) {
        loop_count++;

        if (loop_count % 50000 == 0) {
            led_blink = !led_blink;
            pb_writel(led_blink?0x4000:0, REG(0x020A0000,0));

        }
        

        plat_usb_task();
   }



}
