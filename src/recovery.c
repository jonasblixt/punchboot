#include <recovery.h>
#include <board.h>
#include <tinyprintf.h>
#include <io.h>
#include <usb.h>
#include <crc.h>
#include <emmc.h>
#include <plat.h>
#include <gpt.h>

void pb_flash_bootloader(u8 *bfr, u32 blocks_to_write) {
    
    
    tfp_printf ("Blocks: %i\n\r",blocks_to_write);

    tfp_printf ("Flashing BOOT0\n\r");
    usdhc_emmc_switch_part(1);
    tfp_printf ("Switching to BOOT0\n\r");
    usdhc_emmc_xfer_blocks(2, bfr,blocks_to_write, 1);

    tfp_printf ("Flashing BOOT1\n\r");
    usdhc_emmc_switch_part(2);
    tfp_printf ("Switching to BOOT1\n\r");
    usdhc_emmc_xfer_blocks(2, bfr,blocks_to_write, 1);

    usdhc_emmc_switch_part(0);

}

static void pb_flash_part(u8 part_no, u32 lba_offset, u32 no_of_blocks, u8 *bfr) {
    u32 part_lba_offset = 0;

    part_lba_offset = gpt_get_part_offset(part_no);

    if (!part_lba_offset) {
        tfp_printf ("Unknown partiotion\n\r");
    }

    usdhc_emmc_xfer_blocks(part_lba_offset + lba_offset, bfr, no_of_blocks, 1);
}

void recovery_cmd_event(struct pb_usb_cmd *cmd, u8 *bulk_buffer) {
    u32 *no_of_blks = NULL;
    struct pb_usb_cmd resp;
    struct pb_cmd_write_part *wr_part = (struct pb_cmd_write_part*) cmd;

    switch (cmd->cmd) {
        case PB_CMD_PREP_BULK_BUFFER:
            no_of_blks = (u32 *) cmd->data;
            tfp_printf("Preparting buffer %i\n\r",*no_of_blks);
            plat_usb_prep_bulk_buffer(*no_of_blks);
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
            tfp_printf ("Writing %i blks to part %i with offset %8.8X\n\r",
                        wr_part->no_of_blocks, wr_part->part_no,
                        wr_part->lba_offset);
            pb_flash_part(wr_part->part_no, wr_part->lba_offset, 
                    wr_part->no_of_blocks, bulk_buffer);
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
