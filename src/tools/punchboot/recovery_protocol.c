#include <stdio.h>
#include <pb/recovery.h>
#include <pb/config.h>
#include <pb/gpt.h>
#include <pb/image.h>
#include <uuid/uuid.h>

#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>

#include "recovery_protocol.h"
#include "transport.h"
#include "utils.h"

int pb_install_default_gpt(void) 
{
    return pb_write(PB_CMD_WRITE_DFLT_GPT, NULL, 0);
}

int pb_write_default_fuse(void) 
{
    return pb_write(PB_CMD_WRITE_DFLT_FUSE, NULL, 0);
}

int pb_write_uuid(void) 
{
    uuid_t uuid;
    uuid_generate_random(uuid);
    return pb_write(PB_CMD_WRITE_UUID, (uint8_t *) uuid, 16);
}

int pb_reset(void) 
{
    return pb_write(PB_CMD_RESET, NULL, 0);
}

int pb_boot_part(uint8_t part_no) 
{
    return pb_write(PB_CMD_BOOT_PART, NULL, 0);
}

int pb_print_version(void)
{
    uint32_t sz = 0;
    char version_string[255];
    int err;

    err = pb_write(PB_CMD_GET_VERSION, NULL, 0);

    if (err) {
        return err;
    }

    pb_read((uint8_t*) &sz, 4);
    pb_read((uint8_t*) &version_string, sz);

    version_string[sz] = 0;
    printf ("Version: '%s', l=%u\n",version_string,sz);
    return 0;
}

int pb_get_gpt_table(struct gpt_primary_tbl *tbl) 
{
    uint32_t tbl_sz = 0;
    int err;

    err = pb_write(PB_CMD_GET_GPT_TBL, NULL, 0);

    if (err)
        return err;

    err = pb_read((uint8_t*) &tbl_sz, 4);
    
    if (err)
        return err;

    err = pb_read((uint8_t*) tbl, tbl_sz);

    if (err)
        return err;

    return 0;
}

unsigned int pb_get_config_value(uint32_t index) 
{
    int err;
    int value;
    uint32_t sz;

    err = pb_write(PB_CMD_GET_CONFIG_VAL, (uint8_t *) &index, 4);

    if (err) {
        printf ("Error sending cmd\n");
        return err;
    }

    pb_read((uint8_t *) &sz, 4);
    pb_read((uint8_t *) &value, sz);

    return value;
}

unsigned int pb_set_config_value(uint32_t index, uint32_t val) 
{
    int err;

    uint32_t data[2];
    data[0] = index;
    data[1] = val;

    printf ("Setting %i to %x\n", index, val);
    err = pb_write(PB_CMD_SET_CONFIG_VAL, (uint8_t *) data, 8);

    if (err) {
        printf ("Error sending cmd\n");
        return err;
    }


    return 0;
}

int pb_get_config_tbl (void) 
{
    struct pb_config_item item [127];
    int err;
    uint32_t tbl_sz = 0;
    const char *access_text[] = {"  ","RW","RO","OTP"};

    err = pb_write(PB_CMD_GET_CONFIG_TBL, NULL, 0);

    if (err) {
        printf ("%s: Could not read config table\n",__func__);
        return err;
    }

    err = pb_read((uint8_t *) &tbl_sz, 4);

    err = pb_read((uint8_t *) &item, tbl_sz);

    int n = 0;
    printf (    " Index   Description        Access   Default      Value\n");
    printf (    " -----   -----------        ------  ----------  ----------\n\n");
    do {
        printf (" %-3u     %-16s   %-3s     0x%8.8X  0x%8.8X\n",item[n].index,
                                     item[n].description,
                                     access_text[item[n].access],
                                     item[n].default_value,
                                     pb_get_config_value(n));
        n++;
    } while (item[n].index != -1);

    return 0;
}

int pb_flash_part (uint8_t part_no, const char *f_name) 
{
    int read_sz = 0;
    int sent_sz = 0;
    int buffer_id = 0;
    int err;
    FILE *fp = NULL; 
    unsigned char *bfr = NULL;

    struct pb_cmd_prep_buffer bfr_cmd;
    struct pb_cmd_write_part wr_cmd;

    fp = fopen (f_name,"rb");

    if (fp == NULL) {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    bfr =  malloc(1024*1024*8);
    
    if (bfr == NULL) {
        printf ("Could not allocate memory\n");
        return -1;
    }

    wr_cmd.lba_offset = 0;
    wr_cmd.part_no = part_no;
    printf ("Writing");
    fflush(stdout);
    while ((read_sz = fread(bfr, 1, 1024*1024*8, fp)) >0) {
       bfr_cmd.no_of_blocks = read_sz / 512;
        if (read_sz % 512)
            bfr_cmd.no_of_blocks++;
        
        bfr_cmd.buffer_id = buffer_id;
        pb_write(PB_CMD_PREP_BULK_BUFFER, 
                (uint8_t *) &bfr_cmd, sizeof(struct pb_cmd_prep_buffer));

        printf("read_sz = %u (%u)\n",read_sz, bfr_cmd.no_of_blocks*512);
        err = pb_write_bulk(bfr, bfr_cmd.no_of_blocks*512, &sent_sz);
        if (err != 0) {
            printf ("Bulk xfer error, err=%i\n",err);
            goto err_xfer;
        }

        wr_cmd.no_of_blocks = bfr_cmd.no_of_blocks;
        wr_cmd.buffer_id = buffer_id;
        buffer_id = !buffer_id;
        printf (".");
        fflush(stdout);
        pb_write(PB_CMD_WRITE_PART, (uint8_t *) &wr_cmd,
                    sizeof(struct pb_cmd_write_part));

        wr_cmd.lba_offset += bfr_cmd.no_of_blocks;
 

    }
    printf ("Done\n");
err_xfer:
    free(bfr);
    fclose(fp);
    return err;
}


int pb_program_bootloader (const char *f_name) 
{
    int read_sz = 0;
    int sent_sz = 0;
    int err;
    FILE *fp = NULL; 
    unsigned char bfr[1024*1024*1];
    uint32_t no_of_blocks = 0;
    struct stat finfo;
    struct pb_cmd_prep_buffer buffer_cmd;

    fp = fopen (f_name,"rb");

    if (fp == NULL) {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    stat(f_name, &finfo);

    no_of_blocks = finfo.st_size / 512;

    if (finfo.st_size % 512)
        no_of_blocks++;

    buffer_cmd.buffer_id = 0;
    buffer_cmd.no_of_blocks = no_of_blocks;


    printf ("Installing bootloader, sz = %d blocks\n", 
                    buffer_cmd.no_of_blocks);
    
    pb_write(PB_CMD_PREP_BULK_BUFFER, (uint8_t *) &buffer_cmd,
                                    sizeof(struct pb_cmd_prep_buffer));

    while ((read_sz = fread(bfr, 1, sizeof(bfr), fp)) >0) 
    {
        err = pb_write_bulk(bfr, read_sz, &sent_sz);
        if (err != 0) 
            return -1;
    }
    
    fclose(fp);

    return pb_write(PB_CMD_FLASH_BOOTLOADER, (uint8_t *) &no_of_blocks, 4);
}




int pb_execute_image (const char *f_name) 
{
    int read_sz = 0;
    int sent_sz = 0;
    int err = 0;
    FILE *fp = NULL; 
    unsigned char *bfr = NULL;
    uint32_t data_remaining;
    uint32_t bytes_to_send;
    struct pb_pbi pbi;

    fp = fopen (f_name,"rb");

    if (fp == NULL) {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    bfr =  malloc(1024*64);
    
    if (bfr == NULL) {
        printf ("Could not allocate memory\n");
        return -1;
    }

    read_sz = fread(&pbi, 1, sizeof(struct pb_pbi), fp);

    pb_write(PB_CMD_BOOT_RAM, (uint8_t *) &pbi, sizeof(struct pb_pbi));
    
    printf ("Punchboot Image:\n");
    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) {
        printf (" o %u - LA: 0x%8.8X OFF:0x%8.8X\n",i, 
                            pbi.comp[i].load_addr_low,
                            pbi.comp[i].component_offset);
    }

    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) {
        printf ("Loading component %u, %u bytes...\n",i, 
                                pbi.comp[i].component_size);

        fseek (fp, pbi.comp[i].component_offset, SEEK_SET);
        data_remaining = pbi.comp[i].component_size;

        while ((read_sz = fread(bfr, 1, 1024*64, fp)) >0) {

            if (read_sz > data_remaining)
                bytes_to_send = data_remaining;
            else
                bytes_to_send = read_sz;

            err = pb_write_bulk(bfr, bytes_to_send, &sent_sz);
    
            data_remaining = data_remaining - bytes_to_send;

            if (!data_remaining)
                break;

            if (err != 0) {
                printf ("USB: Bulk xfer error, err=%i\n",err);
                goto err_xfer;
            }
        }
    }

    printf ("Done\n");
err_xfer:
    free(bfr);
    fclose(fp);
    return err;
}

