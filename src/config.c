/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb.h>
#include <stdio.h>
#include <pb/config.h>
#include <pb/gpt.h>
#include <pb/crc.h>
#include <pb/plat.h>

static __no_bss __a4k struct config config;
static __no_bss __a4k struct config config_backup;
static struct gpt_part_hdr * part_config;
static struct gpt_part_hdr * part_config_backup;

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

static void config_defaults(struct config *c)
{
    memset(c, 0, sizeof(struct config));
    c->magic = PB_CONFIG_MAGIC;
}

uint32_t config_init(void)
{
    uint32_t err;
    bool primary_config_ok = false;
    bool backup_config_ok = false;

    err = gpt_get_part_by_uuid(PB_PARTUUID_CONFIG_PRIMARY,
                           &part_config);

    if (err != PB_OK)
    {
        LOG_ERR ("Could not find primary config partition");
        return err;
    }

    err = gpt_get_part_by_uuid(PB_PARTUUID_CONFIG_BACKUP, 
                                 &part_config_backup);

    if (err != PB_OK)
    {
        LOG_ERR ("Could not find backup config partition");
        return err;
    }

    LOG_DBG("Primary config at lba: 0x%llx",part_config->first_lba);
    err = plat_read_block(part_config->first_lba,(uintptr_t) &config,
                        (sizeof(struct config) / 512));

    primary_config_ok = (err == PB_OK)?true:false;

    err = config_validate(&config);

    if (err != PB_OK)
        primary_config_ok = false;

    LOG_DBG("Backup config at lba: 0x%llx",part_config_backup->first_lba);
    err = plat_read_block(part_config_backup->first_lba,
                (uintptr_t) &config_backup, (sizeof(struct config) / 512));

    backup_config_ok = (err == PB_OK)?true:false;

    err = config_validate(&config_backup);

    if (err != PB_OK)
        backup_config_ok = false;

    if (!primary_config_ok && !backup_config_ok)
    {
        LOG_ERR("No valid configuration found, installing default");
        config_defaults(&config);
        config_defaults(&config_backup);
        err = config_commit();
    }
    else if (!backup_config_ok && primary_config_ok)
    {
        LOG_ERR("Backup configuration corrupt, repairing");
        config_defaults(&config_backup);
        err = config_commit();
    }
    else if (backup_config_ok && !primary_config_ok)
    {
        LOG_ERR("Primary configuration corrupt, reparing");
        memcpy(&config, &config_backup, sizeof(struct config));
        err = config_commit();
    }
    else
    {
        LOG_INFO("Configuration loaded");
        err = PB_OK;
    }

    return err;
}

uint32_t config_commit(void)
{
    uint32_t err;

    config.crc = 0;
    uint32_t crc = crc32(0, (const uint8_t *)&config, sizeof(struct config));
    config.crc = crc;

    memcpy(&config_backup, &config, sizeof(struct config));

    err = plat_write_block(part_config->first_lba,
            (uintptr_t) &config, (sizeof(struct config)/512));

    if (err != PB_OK)
        goto config_commit_err;

    err = plat_write_block(part_config_backup->first_lba,
            (uintptr_t) &config, (sizeof(struct config)/512));

config_commit_err:

    if (err != PB_OK)
        LOG_ERR("Could not write configuration");
    else
        LOG_INFO("Configuration written");

    return err;
}

bool config_system_enabled(uint32_t system)
{
    if (system == SYSTEM_A)
    {
        return ((config.enable & PB_CONFIG_A_ENABLED) == PB_CONFIG_A_ENABLED);
    }
    else if (system == SYSTEM_B)
    {
        return ((config.enable & PB_CONFIG_B_ENABLED) == PB_CONFIG_B_ENABLED);
    }
    else
    {
        return false;
    }
}


bool config_system_verified(uint32_t system)
{

    if (system == SYSTEM_A)
    {
        return ((config.verified & PB_CONFIG_A_VERIFIED) == PB_CONFIG_A_VERIFIED);
    }
    else if (system == SYSTEM_B)
    {
        return ((config.verified & PB_CONFIG_B_VERIFIED) == PB_CONFIG_B_VERIFIED);
    }
    else
    {
        return false;
    }
}

uint32_t config_get_remaining_boot_attempts(void)
{
    return config.remaining_boot_attempts;
}

void config_decrement_boot_attempt(void)
{
    if (config.remaining_boot_attempts > 0)
        config.remaining_boot_attempts--;
}

void config_set_boot_error(uint32_t bits)
{
    config.error = bits;
}

void config_system_enable(uint32_t system)
{
    switch (system)
    {
        case SYSTEM_A:
            config.enable = PB_CONFIG_A_ENABLED;;
        break;
        case SYSTEM_B:
            config.enable = PB_CONFIG_B_ENABLED;;
        break;
        default:
            config.enable = 0;
        break;
    };
}


void config_system_set_verified(uint32_t system, bool verified)
{
    switch (system)
    {
        case SYSTEM_A:
            config.remaining_boot_attempts = 0;
            if (verified)
                config.verified |= PB_CONFIG_A_VERIFIED;
            else
                config.verified &= ~PB_CONFIG_A_VERIFIED;
        break;
        case SYSTEM_B:
            config.remaining_boot_attempts = 0;
            if (verified)
                config.verified |= PB_CONFIG_B_VERIFIED;
            else
                config.verified &= ~PB_CONFIG_B_VERIFIED;
        break;
        default:
        break;
    };
}
