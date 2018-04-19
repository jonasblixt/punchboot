#include <recovery.h>
#include <board.h>
#include <tinyprintf.h>
#include <io.h>
#include <usb.h>
#include <crc.h>
#include <emmc.h>
#include <plat.h>

#define BL_MAX_SIZE 1024*1024*4

static u8 usb_buf[4096];
static u8 cmd_to_process;

static u8 __attribute__((section (".bigbuffer"))) chunk_buffer[BL_MAX_SIZE];

void recovery_cmd_event(u8 *bfr, u16 sz) {

//    for (int i = 0; i < sz; i++)
//        usb_buf[i] = bfr[i];

//    cmd_to_process = 1;
}

static void pb_flash_bootloader(u8 *bfr, u32 sz) {
    u32 blocks_to_write = sz / 512;

    if (sz % 512)
        blocks_to_write++;
    


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
    
   


    tfp_printf ("Writing %i blocks to part %i with offset %i...\n\r",no_of_blocks,
                                            part_no, lba_offset);


    usdhc_emmc_xfer_blocks(4096 + lba_offset, bfr, no_of_blocks, 1);
}

void recovery(void) {
    u32 chunk_buf_count = 0;
    

    struct pb_usb_command_hdr *hdr = usb_buf;
    u8 * payload = usb_buf + sizeof(struct pb_usb_command_hdr);
    
    struct pb_chunk_hdr *chunk = (struct pb_chunk_hdr *) payload;
    
    u8 * usb_chunk_buf = usb_buf + sizeof(struct pb_usb_command_hdr) +
                            sizeof(struct pb_chunk_hdr);



    struct pb_write_part_hdr * part_wr_hdr = 
            (struct pb_write_part_hdr *) payload;

    tfp_printf ("\n\r*** RECOVERY MODE ***\n\r");
    
    cmd_to_process = 0;

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

        if (cmd_to_process) {
            cmd_to_process = 0;


            if (crc32(0, usb_buf, sizeof(struct pb_usb_command_hdr)-4) == 
                                                    hdr->header_crc) {


                switch (hdr->cmd) {
                    case PB_CMD_TRANSFER_DATA:
                            /*tfp_printf ("Got chunk: %i, sz=%ib\n\r",
                                        chunk->chunk_no,
                                        chunk->chunk_sz);
                            */
                            if (chunk->chunk_no == 0) {
                                chunk_buf_count = 0;
                            }
                            
                            if (chunk_buf_count + chunk->chunk_sz > 
                                    sizeof(chunk_buffer)) {
                                tfp_printf ("Chunk buffer overflow!\n\r");
                                break;
                            }

                            for (int i = 0; i < chunk->chunk_sz; i++) {
                                chunk_buffer[chunk_buf_count+i] = 
                                        usb_chunk_buf[i];

                            }
                            
                            chunk_buf_count += chunk->chunk_sz;


                        break;
                    case PB_CMD_FLASH_BOOT:
                        tfp_printf ("Installing bootloader  %ib...\n\r",chunk_buf_count);
                        pb_flash_bootloader(chunk_buffer, chunk_buf_count);
                    break;
                    case PB_CMD_WRITE_PART:
                        pb_flash_part(part_wr_hdr->part_no,
                                        part_wr_hdr->lba_offset,
                                        part_wr_hdr->no_of_blocks,
                                        chunk_buffer);
                    break;
                    case PB_CMD_RESET:
                        plat_reset();
                    break;
                    default:

                        tfp_printf ("Got unknown CMD: %2.2X, sz = %ub\n\r",hdr->cmd, hdr->payload_sz);

                };
            }

        }
    }



}
