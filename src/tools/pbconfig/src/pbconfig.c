
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <blkid.h>
#include <pb/pb.h>
#include <pb/config.h>
#include "pbconfig.h"
#include "crc.h"

static struct config config;
static struct config config_backup;
static uint64_t primary_offset, backup_offset;
static const char *device;

static uint32_t config_validate(struct config *c)
{
    uint32_t crc = c->crc;
    uint32_t err = PB_OK;

    c->crc = 0;

    if (c->magic != PB_CONFIG_MAGIC)
    {
        LOG_ERR("Incorrect magic");
        err = PB_ERR;
        goto config_err_out;
    }

    if (crc != crc32(0, (uint8_t *) c, sizeof(struct config)))
    {
        LOG_ERR("CRC failed");
        err = PB_ERR;
        goto config_err_out;
    }

    /* TODO: Perform additional checks ? */

config_err_out:

    return err;
}

static uint32_t pbconfig_commit(void)
{
    uint32_t err = PB_OK;
    uint32_t crc = 0;
    int write_sz;
    FILE *fp = fopen(device, "r+b");

    printf("Writing configuration...\n");

    if (fp == NULL)
    {
        printf("Error: Could not open '%s'\n", device);
        err = PB_ERR_FILE_NOT_FOUND;
        goto commit_err_out1;
    }

    if (fseek(fp, primary_offset*512, SEEK_SET) != 0)
    {
        printf("Error: seek failed\n");
        err = PB_ERR_IO;
        goto commit_err_out;
    }

    config.crc = 0;
    crc = crc32(0, (const uint8_t *) &config, sizeof(struct config));
    config.crc = crc;

    write_sz = fwrite(&config, 1, sizeof(struct config), fp);

    if (write_sz != sizeof(struct config))
    {
        printf("Could not write primary config data");
        err = PB_ERR_IO;
        goto commit_err_out;
    }

    if (fseek(fp, backup_offset*512, SEEK_SET) != 0)
    {
        printf("Error: seek failed\n");
        err = PB_ERR_IO;
        goto commit_err_out;
    }

    write_sz = fwrite(&config, 1, sizeof(struct config), fp);

    if (write_sz != sizeof(struct config))
    {
        printf("Could not write backup config data");
        err = PB_ERR_IO;
        goto commit_err_out;
    }

commit_err_out:
    fclose(fp);
commit_err_out1:
    return err;
}

uint32_t pbconfig_load(const char *_device, uint64_t _primary_offset,
                        uint64_t _backup_offset)
{
    int read_sz = 0;
    uint32_t err = PB_OK;
    device = _device;
    primary_offset = _primary_offset;
    backup_offset = _backup_offset;

    FILE *fp = fopen(device, "rb");

    if (fp == NULL)
    {
        printf("Error: Could not open '%s'\n", device);
        err = PB_ERR_FILE_NOT_FOUND;
        goto load_err_out1;
    }

    if (fseek(fp, primary_offset*512, SEEK_SET) != 0)
    {
        printf("Error: seek failed\n");
        err = PB_ERR_IO;
    }

    read_sz = fread(&config, 1, sizeof(struct config), fp);

    if (read_sz != sizeof(struct config))
    {
        printf("Could not read primary config data");
        err = PB_ERR_IO;
        goto load_err_out;
    }

    if (fseek(fp, backup_offset*512, SEEK_SET) != 0)
    {
        printf("Error: seek failed\n");
        err = PB_ERR_IO;
    }

    read_sz = fread(&config_backup, 1, sizeof(struct config), fp);

    if (read_sz != sizeof(struct config))
    {
        printf("Could not read backup config data");
        err = PB_ERR_IO;
        goto load_err_out;
    }

    if (config_validate(&config) != PB_OK)
    {
        printf("Primary configuration corrupt\n");
        err = PB_ERR;
    }

    if (config_validate(&config_backup) != PB_OK)
    {
        printf("Backup configuration corrupt\n");
        err = PB_ERR;
    }

load_err_out:
    fclose(fp);
load_err_out1:
    return err;
}

void print_configuration(void)
{
    printf("Punchboot status:\n\n");
    printf("System A is %s and %s\n", (config.enable & PB_CONFIG_A_ENABLED)?
                                    "enabled":"disabled",
                                   (config.verified & PB_CONFIG_A_VERIFIED)?
                                    "verified":"not verified");

    printf("System B is %s and %s\n", (config.enable & PB_CONFIG_B_ENABLED)?
                                    "enabled":"disabled",
                                   (config.verified & PB_CONFIG_B_VERIFIED)?
                                    "verified":"not verified");


    printf("Errors : 0x%08x\n", config.error);
    printf("Remaining boot attempts: %u\n", config.remaining_boot_attempts);
}

uint32_t pbconfig_load_from_uuid(void)
{
    return PB_ERR;
}


uint32_t pbconfig_switch(uint8_t system, uint8_t counter)
{
    switch (system)
    {
        case SYSTEM_A:
            config.enable = PB_CONFIG_A_ENABLED;

            if (counter > 0)
            {
                config.remaining_boot_attempts = counter;
                config.verified &= ~PB_CONFIG_A_VERIFIED;
                config.error = 0;
            }
            else
            {
                config.verified |= PB_CONFIG_A_VERIFIED;
                config.remaining_boot_attempts = 0;
                config.error = 0;
            }
        break;
        case SYSTEM_B:
            config.enable = PB_CONFIG_B_ENABLED;

            if (counter > 0)
            {
                config.remaining_boot_attempts = counter;
                config.verified &= ~PB_CONFIG_B_VERIFIED;
                config.error = 0;
            }
            else
            {
                config.verified |= PB_CONFIG_B_VERIFIED;
                config.remaining_boot_attempts = 0;
                config.error = 0;
            }
        break;
        case SYSTEM_NONE:
            config.enable = 0;
            config.remaining_boot_attempts = 0;
        break;
        default:
            return PB_ERR;
        break;
    }

    return pbconfig_commit();
}

uint32_t pbconfig_set_verified(uint8_t system)
{
    switch (system)
    {
        case SYSTEM_A:
            config.verified |= PB_CONFIG_A_VERIFIED;
            config.remaining_boot_attempts = 0;
        break;
        case SYSTEM_B:
            config.verified |= PB_CONFIG_B_VERIFIED;
            config.remaining_boot_attempts = 0;
        break;
        case SYSTEM_NONE:
        break;
        default:
            return PB_ERR;
        break;
    }
    return pbconfig_commit();
}
