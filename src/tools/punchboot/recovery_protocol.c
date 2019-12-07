/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/recovery.h>
#include <pb/gpt.h>
#include <pb/image.h>
#include <uuid.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>

#include <3pp/bearssl/bearssl_hash.h>
#include <pb/crypto.h>
#include "recovery_protocol.h"
#include "transport.h"
#include "utils.h"


uint32_t pb_recovery_authenticate(uint32_t key_index, const char *fn,
                                  uint32_t signature_kind, uint32_t hash_kind)
{
    uint32_t err;
    uint8_t cookie_buffer[PB_RECOVERY_AUTH_COOKIE_SZ];
    unsigned char signature[PB_IMAGE_SIGN_MAX_SIZE];

    memset(cookie_buffer, 0, PB_RECOVERY_AUTH_COOKIE_SZ);
    memset(signature, 0, PB_IMAGE_SIGN_MAX_SIZE);

    FILE *fp = fopen(fn, "rb");

    if (fp == NULL)
        return PB_ERR_FILE_NOT_FOUND;

    int read_sz = fread(cookie_buffer, 1, PB_RECOVERY_AUTH_COOKIE_SZ, fp);


    printf("Read %i bytes\n", read_sz);

    /* Signature output from openssl is ASN.1 encoded
     * punchboot requires raw ECDSA signatures
     * RSA signatures should retain the ASN.1 structure
     * */
    uint8_t r_sz = 0;
    uint8_t s_sz = 0;
    uint8_t r_off = 0;
    uint8_t s_off = 0;

    switch (signature_kind)
    {
        case PB_SIGN_NIST256p:
            r_sz = cookie_buffer[3];
            r_off = r_sz-32;
            s_sz = cookie_buffer[3+r_sz+2];
            s_off = s_sz-32;
            memcpy(&signature[0], &cookie_buffer[3+1+r_off], r_sz);
            memcpy(&signature[32], &cookie_buffer[3+r_sz+2+1+s_off], s_sz);
            read_sz = r_sz+s_sz+r_off+s_off;
        break;
        case PB_SIGN_NIST384p:
            r_sz = cookie_buffer[3];
            r_off = r_sz-48;
            s_sz = cookie_buffer[3+r_sz+2];
            s_off = s_sz-48;
            memcpy(&signature[0], &cookie_buffer[4+r_off], r_sz);
            memcpy(&signature[48], &cookie_buffer[3+r_sz+2+1+s_off], s_sz);
            read_sz = r_sz+s_sz+r_off+s_off;
        break;
        case PB_SIGN_NIST521p:
            r_sz = cookie_buffer[4];
            r_off = 66-r_sz;
            s_sz = cookie_buffer[4+r_sz+2];
            s_off = 66-s_sz;
            memcpy(&signature[r_off], &cookie_buffer[5], r_sz);
            memcpy(&signature[66+s_off], &cookie_buffer[4+r_sz+2+1], s_sz);
            read_sz = r_sz+s_sz+r_off+s_off;
        break;
        case PB_SIGN_RSA4096:
            memcpy(signature, cookie_buffer, read_sz);
        break;
        default:
            printf("Unknown signature format\n");
            return PB_ERR;
    }

    err = pb_write(PB_CMD_AUTHENTICATE, key_index, signature_kind, hash_kind,
                                        0, signature, read_sz);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_recovery_setup_lock(void)
{
    uint32_t err;

    err = pb_write(PB_CMD_SETUP_LOCK, 0, 0, 0, 0, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_recovery_setup(struct param *params)
{
    uint32_t err = PB_ERR;
    uint32_t param_count = 0;

    while (params[param_count].kind != PB_PARAM_END)
        param_count++;

    err = pb_write(PB_CMD_SETUP, param_count, 0, 0, 0, (uint8_t *) params,
                            (sizeof(struct param)*param_count));

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_read_params(struct param **params)
{
    uint32_t sz;
    uint32_t err;

    err = pb_write(PB_CMD_GET_PARAMS, 0, 0, 0, 0, NULL, 0);

    if (err != PB_OK)
        return err;

    err = pb_read((uint8_t *) &sz, sizeof(uint32_t));

    if (err != PB_OK)
        return err;

    (*params) = malloc(sz);

    err = pb_read((uint8_t *) (*params), sz);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_install_default_gpt(void)
{
    if (pb_write(PB_CMD_WRITE_DFLT_GPT, 0, 0, 0, 0, NULL, 0) != PB_OK)
        return PB_ERR;

    return pb_read_result_code();
}


uint32_t pb_reset(void)
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_RESET, 0, 0, 0, 0, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_boot_part(uint8_t part_no, bool verbose)
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_BOOT_PART, part_no, verbose, 0, 0, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_is_auhenticated(bool *result)
{
    uint8_t tmp = 0;
    uint32_t err;
    uint32_t length;

    *result = false;

    err = pb_write(PB_CMD_IS_AUTHENTICATED, 0, 0, 0, 0, NULL, 0);

    if (err != PB_OK)
        return err;

    err = pb_read((uint8_t *)&length, 4);

    if (err != PB_OK)
        return err;

    if (length != 1)
        return PB_ERR;

    err = pb_read(&tmp, 1);

    if (err != PB_OK)
        return err;

    if (tmp == 1)
        *result = true;
    else
        *result = false;

    return pb_read_result_code();
}

uint32_t pb_set_bootpart(uint8_t bootpart)
{
    uint32_t err = PB_ERR;

    err = pb_write(PB_CMD_BOOT_ACTIVATE, bootpart, 0, 0, 0, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}

uint32_t pb_get_version(char **out)
{
    uint32_t sz = 0;
    int err;

    err = pb_write(PB_CMD_GET_VERSION, 0, 0, 0, 0, NULL, 0);

    if (err)
        return err;

    err = pb_read((uint8_t*) &sz, 4);

    if (sz == 0)
        return PB_ERR;

    *out = malloc(sz+1);
    bzero(*out, sz+1);

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

    (*out)[sz] = 0;

    err = pb_read_result_code();

    if (err != PB_OK)
        free(*out);

    return err;
}

uint32_t pb_get_gpt_table(struct gpt_primary_tbl *tbl)
{
    uint32_t tbl_sz = 0;
    int err;

    err = pb_write(PB_CMD_GET_GPT_TBL, 0, 0, 0, 0, NULL, 0);

    if (err)
        return err;

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    err = pb_read((uint8_t*) &tbl_sz, 4);

    if (err)
        return err;

    err = pb_read((uint8_t*) tbl, tbl_sz);

    if (err)
        return err;

    return pb_read_result_code();
}

uint32_t pb_check_part(uint8_t part_no, int64_t offset, const char *f_name)
{
    br_sha256_context ctx;
    int rc = PB_OK;
    char hash_data[32];
    char *buf = malloc(1024*1024);
    
    if (!buf)
    {
        rc = PB_ERR;
        goto err_out;
    }

    FILE *fp = fopen(f_name, "rb");

    if (!fp)
    {
        rc = PB_ERR;
        goto err_free_buf;
    }

    br_sha256_init(&ctx);
    
    size_t read_data = 0;
    size_t file_size = 0;
    do
    {
        read_data = fread(buf, 1, 1024*1024, fp);
        file_size += read_data;
        br_sha256_update(&ctx, buf, read_data);
    } while (read_data);

    br_sha256_out(&ctx, hash_data);

    rc = pb_write(PB_CMD_VERIFY_PART, part_no, offset, file_size, 0,
                    (uint8_t *) hash_data, 32);

    if (rc != PB_OK)
        goto err_close_fp;

    rc = pb_read_result_code();

    if (rc != PB_OK)
        goto err_close_fp;

err_close_fp:
    fclose(fp);
err_free_buf:
    free(buf);
err_out:
    return rc;
}

uint32_t pb_flash_part(uint8_t part_no, int64_t offset, const char *f_name)
{
    int read_sz = 0;
    int sent_sz = 0;
    int buffer_id = 0;
    int err;
    FILE *fp = NULL;
    unsigned char *bfr = NULL;

    struct pb_cmd_prep_buffer bfr_cmd;
    struct pb_cmd_write_part wr_cmd;

    fp = fopen(f_name, "rb");

    if (fp == NULL)
    {
        printf("Could not open file: %s\n", f_name);
        return PB_ERR;
    }

    bfr =  malloc(1024*1024*4);

    if (bfr == NULL)
    {
        printf("Could not allocate memory\n");
        return PB_ERR;
    }

    wr_cmd.lba_offset = offset;
    wr_cmd.part_no = part_no;

    while ((read_sz = fread(bfr, 1, 1024*1024*4, fp)) >0)
    {
       bfr_cmd.no_of_blocks = read_sz / 512;
        if (read_sz % 512)
            bfr_cmd.no_of_blocks++;

        bfr_cmd.buffer_id = buffer_id;
        err = pb_write(PB_CMD_PREP_BULK_BUFFER, 0, 0, 0, 0,
                (uint8_t *) &bfr_cmd, sizeof(struct pb_cmd_prep_buffer));

        if (err != PB_OK)
            return err;

        err = pb_write_bulk(bfr, bfr_cmd.no_of_blocks*512, &sent_sz);

        if (err != 0)
        {
            printf("Bulk xfer error, err=%i\n", err);
            goto err_xfer;
        }

        err = pb_read_result_code();

        if (err != PB_OK)
            return err;

        wr_cmd.no_of_blocks = bfr_cmd.no_of_blocks;
        wr_cmd.buffer_id = buffer_id;
        buffer_id = !buffer_id;

        pb_write(PB_CMD_WRITE_PART, 0, 0, 0, 0, (uint8_t *) &wr_cmd,
                    sizeof(struct pb_cmd_write_part));

        err = pb_read_result_code();

        if (err != PB_OK)
            return err;

        wr_cmd.lba_offset += bfr_cmd.no_of_blocks;
    }

    pb_write(PB_CMD_WRITE_PART_FINAL, 0, 0, 0, 0, NULL, 0);
    err = pb_read_result_code();

err_xfer:
    free(bfr);
    fclose(fp);
    return err;
}


uint32_t pb_program_bootloader(const char *f_name)
{
    int read_sz = 0;
    int sent_sz = 0;
    int err;
    FILE *fp = NULL;
    unsigned char bfr[1024*1024*1];
    uint32_t no_of_blocks = 0;
    char hash[32];
    struct stat finfo;
    struct pb_cmd_prep_buffer buffer_cmd;
    br_sha256_context ctx;

    fp = fopen(f_name, "rb");

    if (fp == NULL)
    {
        printf("Could not open file: %s\n", f_name);
        return -1;
    }

    stat(f_name, &finfo);

    no_of_blocks = finfo.st_size / 512;

    if (finfo.st_size % 512)
        no_of_blocks++;

    buffer_cmd.buffer_id = 0;
    buffer_cmd.no_of_blocks = no_of_blocks;

    err = pb_write(PB_CMD_PREP_BULK_BUFFER, 0, 0, 0, 0, (uint8_t *) &buffer_cmd,
                                    sizeof(struct pb_cmd_prep_buffer));

    if (err != PB_OK)
        return err;

    br_sha256_init(&ctx);

    while ((read_sz = fread(bfr, 1, sizeof(bfr), fp)) >0)
    {
        err = pb_write_bulk(bfr, read_sz, &sent_sz);
        br_sha256_update(&ctx, bfr, read_sz);
        if (err != 0)
            return -1;
    }

    br_sha256_out(&ctx, hash);
    fclose(fp);

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    err = pb_write(PB_CMD_FLASH_BOOTLOADER, no_of_blocks, finfo.st_size, 0, 0,
                    (uint8_t *) hash, 32);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}




uint32_t pb_execute_image(const char *f_name, uint32_t active_system,
                            bool verbose)
{
    int read_sz = 0;
    int sent_sz = 0;
    int err = 0;
    FILE *fp = NULL;
    unsigned char *bfr = NULL;
    uint32_t data_remaining;
    uint32_t bytes_to_send;
    struct pb_pbi pbi;
    uint8_t zero_padding[511];

    memset(zero_padding, 0, 511);

    fp = fopen(f_name, "rb");

    if (fp == NULL)
    {
        printf("Could not open file: %s\n", f_name);
        return -1;
    }

    bfr =  malloc(1024*1024);

    if (bfr == NULL)
    {
        printf("Could not allocate memory\n");
        return -1;
    }

    read_sz = fread(&pbi, 1, sizeof(struct pb_pbi), fp);

    err = pb_write(PB_CMD_BOOT_RAM, active_system, verbose, 0, 0,
                            (uint8_t *) &pbi, sizeof(struct pb_pbi));

    if (err != PB_OK)
        return err;

    err = pb_read_result_code();

    if (err != PB_OK)
        return err;

    for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++)
    {
        fseek(fp, pbi.comp[i].component_offset, SEEK_SET);
        data_remaining = pbi.comp[i].component_size;

        while ((read_sz = fread(bfr, 1, 1024*1024, fp)) >0) {
            if (read_sz > data_remaining)
                bytes_to_send = data_remaining;
            else
                bytes_to_send = read_sz;

            err = pb_write_bulk(bfr, bytes_to_send, &sent_sz);

            data_remaining = data_remaining - bytes_to_send;

            if (!data_remaining)
                break;

            if (err != 0) {
                printf("USB: Bulk xfer error, err=%i\n", err);
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

uint32_t pb_recovery_board_command(uint32_t arg0, uint32_t arg1, uint32_t arg2,
                                   uint32_t arg3)
{
    uint32_t err;

    err = pb_write(PB_CMD_BOARD_COMMAND, arg0, arg1, arg2, arg3, NULL, 0);

    if (err != PB_OK)
        return err;

    return pb_read_result_code();
}
