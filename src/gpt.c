#include <board.h>
#include <emmc.h>
#include <gpt.h>
#include <tinyprintf.h>
#include <crc.h>

static u8 __attribute__((section (".bigbuffer"))) _gpt_bfr[1024];
static struct gpt_header hdr;


void gpt_init(void) {
    u32 crc_tmp;
    u32 crc_calc;

    usdhc_emmc_xfer_blocks(1, _gpt_bfr, 1, 0);
    u8 * hdr_ptr = (u8 *) &hdr;
    for (int i = 0; i < sizeof(struct gpt_header); i++)
        *hdr_ptr++ = _gpt_bfr[i];

    tfp_printf ("GPT: ");
    for (int i = 0; i < 8;i++)
        tfp_printf("%c", hdr.signature[i]);
    tfp_printf("\n\r");
    
    crc_tmp = hdr.hdr_crc;
    hdr.hdr_crc = 0;
    crc_calc = crc32(0, &hdr, hdr.hdr_sz);

    tfp_printf ("0x%8.8X - 0x%8.8X\n\r",crc_tmp, crc_calc);
    if (crc_calc != crc_tmp) {
        tfp_printf ("Header CRC error\n\r");
    }

    tfp_printf ("Entries LBA: 0x%8.8X\n\r",hdr.entries_start_lba);
    tfp_printf ("Parts: %i\n\r",hdr.no_of_parts);
    tfp_printf ("Part entry sz: %i\n\r",hdr.part_entry_sz);


    usdhc_emmc_xfer_blocks(hdr.entries_start_lba, _gpt_bfr, 1, 0);

    struct gpt_part_hdr *part = _gpt_bfr;

    for (int i = 0; i < 16; i++) 
        tfp_printf ("%2.2x", part->type_uuid[i]);
    tfp_printf ("\n\r");

    for (int i = 0; i < 16; i++) 
        tfp_printf ("%c",part->name[i]);
    tfp_printf("\n\r");
    tfp_printf ("Start LBA: %i\n\r", part->first_lba);
    part++;

    for (int i = 0; i < 16; i++) 
        tfp_printf ("%2.2x", part->type_uuid[i]);
    tfp_printf ("\n\r");

    for (int i = 0; i < 16; i++) 
        tfp_printf ("%c",part->name[i]);
    tfp_printf("\n\r");
    tfp_printf ("Start LBA: %i\n\r", part->first_lba);
 
    part++;

    for (int i = 0; i < 16; i++) 
        tfp_printf ("%2.2x", part->type_uuid[i]);
    tfp_printf ("\n\r");

    for (int i = 0; i < 16; i++) 
        tfp_printf ("%c",part->name[i]);
    tfp_printf("\n\r");
    tfp_printf ("Start LBA: %i\n\r", part->first_lba);
 

}
