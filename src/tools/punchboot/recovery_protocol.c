#include <stdio.h>
#include <pb/pb.h>
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



uint32_t pb_install_default_gpt(void) 
{
    if (pb_write(PB_CMD_WRITE_DFLT_GPT, NULL, 0) != PB_OK)
        return PB_ERR;
    
    return pb_read_result_code();
}

uint32_t pb_write_default_fuse(void) 
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_WRITE_DFLT_FUSE, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_write_uuid(void) 
{
    uuid_t uuid;
    uint32_t err = PB_ERR;

    uuid_generate_random(uuid);
    err = pb_write(PB_CMD_WRITE_UUID, (uint8_t *) uuid, 16);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}


uint32_t pb_read_uuid(uint8_t *uuid)
{
    uint32_t err = PB_ERR;
    uint32_t sz = 0;

    err = pb_write(PB_CMD_READ_UUID, NULL, 0);

    if (err != PB_OK)
        return err;

    err = pb_read((uint8_t *)&sz, sizeof(uint32_t));

    if (sz != 16)
        return PB_ERR;

    err = pb_read(uuid, 16);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_reset(void) 
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_RESET, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_boot_part(uint8_t part_no) 
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_BOOT_PART, &part_no, sizeof(uint8_t));
    
    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_get_version(char **out)
{
    uint32_t sz = 0;
    int err;

    err = pb_write(PB_CMD_GET_VERSION, NULL, 0);

    if (err)
        return err;

    err = pb_read((uint8_t*) &sz, 4);

    *out = malloc(sz+1);

    if (err)
    {
        free(*out);
        return err;
    }

    err = pb_read((uint8_t*) *out, sz);

    if (err)
    {
        free(*out);
        return err;
    }

    out[sz] = 0;

    err = pb_read_result_code();
    if (err)
        free(*out);
    return err;
}

uint32_t pb_get_gpt_table(struct gpt_primary_tbl *tbl) 
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

    return pb_read_result_code();
}

uint32_t pb_get_config_value(uint32_t index, uint32_t *value) 
{
    int err;
    uint32_t sz;

    err = pb_write(PB_CMD_GET_CONFIG_VAL, (uint8_t *) &index, 4);

    if (err)
        return err;

    err = pb_read((uint8_t *) &sz, 4);

    if (err)
        return err;

    err = pb_read((uint8_t *) value, sz);

    if (err)
        return err;

    return pb_read_result_code();
}

uint32_t pb_set_config_value(uint32_t index, uint32_t val) 
{
    int err;
    uint32_t data[2];
    data[0] = index;
    data[1] = val;

    err = pb_write(PB_CMD_SET_CONFIG_VAL, (uint8_t *) data, 8);

    if (err)
        return err;

    return pb_read_result_code();
}

uint32_t pb_get_config_tbl (struct pb_config_item *items) 
{
    int err;
    uint32_t tbl_sz = 0;

    err = pb_write(PB_CMD_GET_CONFIG_TBL, NULL, 0);

    if (err) 
        return err;

    err = pb_read((uint8_t *) &tbl_sz, 4);

    if (err) 
        return err;

    err = pb_read((uint8_t *) items, tbl_sz);

    if (err) 
        return err;

    return pb_read_result_code();
}

uint32_t pb_flash_part (uint8_t part_no, const char *f_name) 
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

    if (fp == NULL) 
    {
        printf ("Could not open file: %s\n",f_name);
        return PB_ERR;
    }

    bfr =  malloc(1024*1024*8);
    
    if (bfr == NULL) 
    {
        printf ("Could not allocate memory\n");
        return PB_ERR;
    }

    wr_cmd.lba_offset = 0;
    wr_cmd.part_no = part_no;

    while ((read_sz = fread(bfr, 1, 1024*1024*4, fp)) >0) {
       bfr_cmd.no_of_blocks = read_sz / 512;
        if (read_sz % 512)
            bfr_cmd.no_of_blocks++;
        
        bfr_cmd.buffer_id = buffer_id;
        pb_write(PB_CMD_PREP_BULK_BUFFER, 
                (uint8_t *) &bfr_cmd, sizeof(struct pb_cmd_prep_buffer));

        err = pb_write_bulk(bfr, bfr_cmd.no_of_blocks*512, &sent_sz);
        if (err != 0) {
            printf ("Bulk xfer error, err=%i\n",err);
            goto err_xfer;
        }

        err = pb_read_result_code();

        if (err != PB_OK)
            return err;

        wr_cmd.no_of_blocks = bfr_cmd.no_of_blocks;
        wr_cmd.buffer_id = buffer_id;
        buffer_id = !buffer_id;

        pb_write(PB_CMD_WRITE_PART, (uint8_t *) &wr_cmd,
                    sizeof(struct pb_cmd_write_part));

        err = pb_read_result_code();

        if (err != PB_OK)
            return err;

        wr_cmd.lba_offset += bfr_cmd.no_of_blocks;
    }

err_xfer:
    free(bfr);
    fclose(fp);
    return err;
}


uint32_t pb_program_bootloader (const char *f_name) 
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

    if (fp == NULL) 
    {
        printf ("Could not open file: %s\n",f_name);
        return -1;
    }

    stat(f_name, &finfo);

    no_of_blocks = finfo.st_size / 512;

    if (finfo.st_size % 512)
        no_of_blocks++;

    buffer_cmd.buffer_id = 0;
    buffer_cmd.no_of_blocks = no_of_blocks;
    
    pb_write(PB_CMD_PREP_BULK_BUFFER, (uint8_t *) &buffer_cmd,
                                    sizeof(struct pb_cmd_prep_buffer));

    while ((read_sz = fread(bfr, 1, sizeof(bfr), fp)) >0) 
    {
        err = pb_write_bulk(bfr, read_sz, &sent_sz);
        if (err != 0) 
            return -1;
    }
    
    fclose(fp);

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    err = pb_write(PB_CMD_FLASH_BOOTLOADER, (uint8_t *) &no_of_blocks, 4);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}




uint32_t pb_execute_image (const char *f_name) 
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

    err = pb_write(PB_CMD_BOOT_RAM, (uint8_t *) &pbi, sizeof(struct pb_pbi));
    
    if (err != PB_OK)
        return err;

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) {

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

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

err_xfer:


    free(bfr);
    fclose(fp);
    return err;
}

