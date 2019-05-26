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

uint32_t config_init(struct gpt *gpt)
{
    uint32_t err;
    bool primary_config_ok = false;
    bool backup_config_ok = false;

    err = gpt_get_part_by_uuid(gpt, PB_PARTUUID_CONFIG_PRIMARY,
                           &part_config);

    if (err != PB_OK)
    {
        LOG_ERR ("Could not find primary config partition");
        return err;
    }

    err = gpt_get_part_by_uuid(gpt, PB_PARTUUID_CONFIG_BACKUP, 
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
        return (config.a_sys_enable==1);
    }
    else if (system == SYSTEM_B)
    {
        return (config.b_sys_enable == 1);
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
        return (config.a_sys_verified ==1);
    }
    else if (system == SYSTEM_B)
    {
        return (config.b_sys_verified == 1);
    }
    else
    {
        return false;
    }
}
uint32_t config_get_boot_counter(uint32_t system)
{
    uint8_t counter = 0;

    switch (system)
    {
        case SYSTEM_A:
            counter = config.a_boot_counter;
        break;
        case SYSTEM_B:
            counter = config.b_boot_counter;
        break;
        default:
            counter = 0;
    };

    return counter;
}

void config_set_boot_counter(uint32_t system, uint8_t counter)
{

    switch (system)
    {
        case SYSTEM_A:
            config.a_boot_counter = counter;
        break;
        case SYSTEM_B:
            config.b_boot_counter = counter;
        break;
        default:
        break;
    };
}

void config_set_boot_error_bits(uint32_t system, uint32_t bits)
{
    switch (system)
    {
        case SYSTEM_A:
            config.a_boot_error = bits;
        break;
        case SYSTEM_B:
            config.b_boot_error = bits;
        break;
        default:
        break;
    };
}

void config_system_enable(uint32_t system, bool enable)
{
    switch (system)
    {
        case SYSTEM_A:
            config.a_sys_enable = enable;
        break;
        case SYSTEM_B:
            config.b_sys_enable = enable;
        break;
        default:
        break;
    };
}


void config_system_set_verified(uint32_t system, bool verified)
{
    switch (system)
    {
        case SYSTEM_A:
            config.a_sys_verified = verified;
        break;
        case SYSTEM_B:
            config.b_sys_verified = verified;
        break;
        default:
        break;
    };
}
