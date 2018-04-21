#include <board.h>
#include <emmc.h>
#include <gpt.h>
#include <tinyprintf.h>
#include <crc.h>

static struct gpt_primary_tbl __attribute__((section (".bigbuffer"))) _gpt1;


static void gpt_part_name(struct gpt_part_hdr *part, u8 *out, u8 len) {
    u8 null_count = 0;

    for (int i = 0; i < len*2; i++) {
        if (part->name[i]) {
            *out++ = part->name[i]; 
            null_count = 0;
        } else {
            null_count++;
        }

        *out = 0;

        if (null_count > 1)
            break;
    }
}

struct gpt_primary_tbl * gpt_get_tbl(void) {
    return &_gpt1;
}

u32 gpt_get_part_offset(u8 part_no) {
    return _gpt1.part[part_no & 0x1f].first_lba;
}

int gpt_init(void) {
    u8 tmp_string[64];

    usdhc_emmc_xfer_blocks(1,(u8*) &_gpt1, 
                    sizeof(struct gpt_primary_tbl) / 512, 0, 0);

/*    tfp_printf("GPT: Init...\n\r");

    for (int i = 0; i < _gpt1.hdr.no_of_parts; i++) {

        if (_gpt1.part[i].first_lba == 0)
            break;

        gpt_part_name(&_gpt1.part[i], tmp_string, sizeof(tmp_string));

        tfp_printf (" %i - [%16s] lba 0x%8.8X - 0x%8.8X\n\r",
                        i,
                        tmp_string,
                        _gpt1.part[i].first_lba,
                        _gpt1.part[i].last_lba);

        
    }
*/
    return 0;
}
