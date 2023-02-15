
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pbstate.h"
#include "crc.h"

#define PB_STATE_MAGIC 0x026d4a65

struct pb_boot_state /* 512 bytes */
{
    uint32_t magic;
    uint32_t enable;
    uint32_t verified;
    uint32_t remaining_boot_attempts;
    uint32_t error;
    uint8_t private[452];
    uint8_t rz[36];
    uint32_t crc;
} __attribute__((packed));

#define PB_STATE_A_ENABLED (1 << 0)
#define PB_STATE_B_ENABLED (1 << 1)
#define PB_STATE_A_VERIFIED (1 << 0)
#define PB_STATE_B_VERIFIED (1 << 1)

static struct pb_boot_state config;
static struct pb_boot_state config_backup;
static uint64_t primary_offset, backup_offset;
static const char *primary_device;
static const char *backup_device;

static pbstate_printfunc_t printfunc;

#define LOG(fmt, ...) \
    if (printfunc) \
        printfunc(fmt, ##__VA_ARGS__)

static int config_validate(struct pb_boot_state *c)
{
    uint32_t crc = c->crc;
    int err = 0;

    c->crc = 0;

    if (c->magic != PB_STATE_MAGIC)
    {
        LOG("Error: Incorrect magic\n");
        err = -1;
        goto config_err_out;
    }

    if (crc != crc32(0, (uint8_t *) c, sizeof(struct pb_boot_state)))
    {
        LOG("Error: CRC failed\n");
        err = -1;
        goto config_err_out;
    }

    /* TODO: Perform additional checks ? */

config_err_out:

    return err;
}

static int write_config(int fd, const struct pb_boot_state* config)
{
    size_t write_sz = 0;
    const char* buffer = (const char*) config;
    size_t buffer_len = sizeof(*config);

    do {
        ssize_t write_now = write(fd, buffer + write_sz, buffer_len - write_sz);
        if (write_now == -1) {
            return -1;
        }
        write_sz += write_now;
    } while (write_sz < buffer_len);

    return 0;
}

static int pbstate_commit(void)
{
    int err = 0;
    uint32_t crc = 0;
    int write_sz;
    int fd = open(primary_device, O_WRONLY | O_DSYNC);

    LOG("Writing configuration...\n");

    if (fd == -1)
    {
        LOG("Error: Could not open '%s'\n", primary_device);
        err = -1;
        goto commit_err_out1;
    }

    config.crc = 0;
    crc = crc32(0, (const uint8_t *) &config, sizeof(struct pb_boot_state));
    config.crc = crc;

    if (write_config(fd, &config) == -1)
    {
        LOG("Could not write primary config data\n");
        err = -1;
        goto commit_err_out;
    }

    close(fd);
    fd = open(backup_device, O_WRONLY | O_DSYNC);

    if (fd == -1)
    {
        LOG("Error: Could not open '%s'\n", backup_device);
        err = -1;
        goto commit_err_out1;
    }

    /* Synchronize backup partition with primary partition */
    if (write_config(fd, &config) == -1)
    {
        LOG("Could not write backup config data\n");
        err = -1;
        goto commit_err_out;
    }

    sync(); /* Ensure that data is written to disk */

commit_err_out:
    close(fd);
commit_err_out1:
    return err;
}

static int read_config(int fd, struct pb_boot_state* config)
{
    size_t read_sz = 0;
    char* buffer = (char*) config;
    size_t buffer_len = sizeof(*config);

    do {
        ssize_t read_now = read(fd, buffer + read_sz, buffer_len - read_sz);
        if (read_now == 0)
        {
            errno = EIO;
            return -1;
        }
        else if (read_now == -1)
        {
            return -1;
        }
        read_sz += read_now;
    } while (read_sz < buffer_len);

    return 0;
}

int pbstate_load(const char *p_device,
                 const char *b_device,
                 pbstate_printfunc_t _printfunc)
{
    int err = 0;
    int fd = 0;
    bool primary_ok;
    bool backup_ok;

    primary_device = p_device;
    backup_device = b_device;
    printfunc = _printfunc;

    fd = open(primary_device, O_RDONLY);

    if (fd == -1)
    {
        LOG("Error: Could not open '%s'\n", primary_device);
        return -1;
    }

    if (read_config(fd, &config) != 0)
    {
        LOG("Could not read primary config data\n");
    }

    close(fd);
    fd = open(backup_device, O_RDONLY);

    if (fd == -1)
    {
        LOG("Error: Could not open '%s'\n", primary_device);
        return -1;
    }

    if (read_config(fd, &config_backup) != 0)
    {
        LOG("Could not read backup config data\n");
    }

    close(fd);

    if (config_validate(&config) == 0)
        primary_ok = true;
    else
        primary_ok = false;

    if (config_validate(&config_backup) == 0)
        backup_ok = true;
    else
        backup_ok = false;

    if (!primary_ok && backup_ok)
    {
        LOG("Primary configuration corrupt, using backup\n");
        memcpy(&config, &config_backup, sizeof(config));
    }
    else if (!primary_ok)
    {
        LOG("Primary configuration corrupt\n");
        LOG("Backup configuration corrupt\n");
        err = -1;
    }

    return err;
}

bool pbstate_is_system_active(pbstate_system_t system)
{
    switch (system)
    {
        case PBSTATE_SYSTEM_A:
            return config.enable == PB_STATE_A_ENABLED;
        case PBSTATE_SYSTEM_B:
            return config.enable == PB_STATE_B_ENABLED;
    }

    return false;
}

bool pbstate_is_system_verified(pbstate_system_t system)
{
    switch (system)
    {
        case PBSTATE_SYSTEM_A:
            return !!(config.verified & PB_STATE_A_VERIFIED);
        case PBSTATE_SYSTEM_B:
            return !!(config.verified & PB_STATE_B_VERIFIED);
    }

    return false;
}

uint32_t pbstate_get_boot_attempts(void)
{
    return config.remaining_boot_attempts;
}

int pbstate_force_rollback(void)
{
    /* Rolling back a verified system is not allowed */
    if (pbstate_is_system_active(PBSTATE_SYSTEM_A)
        && pbstate_is_system_verified(PBSTATE_SYSTEM_A))
    {
        errno = EPERM;
        return -1;
    }
    if (pbstate_is_system_active(PBSTATE_SYSTEM_B)
        && pbstate_is_system_verified(PBSTATE_SYSTEM_B))
    {
        errno = EPERM;
        return -1;
    }

    config.remaining_boot_attempts = 0;

    return pbstate_commit();
}

uint32_t pbstate_get_errors(void)
{
    return config.error;
}

int pbstate_clear_error(uint32_t error_flag)
{
    config.error &= ~error_flag;

    return pbstate_commit();
}

int pbstate_switch_system(pbstate_system_t system, uint32_t boot_attempts)
{
    switch (system)
    {
        case PBSTATE_SYSTEM_A:
            config.enable = PB_STATE_A_ENABLED;

            if (boot_attempts > 0)
            {
                config.remaining_boot_attempts = boot_attempts;
                config.verified &= ~PB_STATE_A_VERIFIED;
                config.error = 0;
            }
            else
            {
                config.verified |= PB_STATE_A_VERIFIED;
                config.remaining_boot_attempts = 0;
                config.error = 0;
            }
        break;
        case PBSTATE_SYSTEM_B:
            config.enable = PB_STATE_B_ENABLED;

            if (boot_attempts > 0)
            {
                config.remaining_boot_attempts = boot_attempts;
                config.verified &= ~PB_STATE_B_VERIFIED;
                config.error = 0;
            }
            else
            {
                config.verified |= PB_STATE_B_VERIFIED;
                config.remaining_boot_attempts = 0;
                config.error = 0;
            }
        break;
        case PBSTATE_SYSTEM_NONE:
            config.enable = 0;
            config.remaining_boot_attempts = 0;
        break;
        default:
            return -1;
        break;
    }

    return pbstate_commit();
}

int pbstate_set_system_verified(pbstate_system_t system)
{
    switch (system)
    {
        case PBSTATE_SYSTEM_A:
            config.verified |= PB_STATE_A_VERIFIED;
            config.remaining_boot_attempts = 0;
        break;
        case PBSTATE_SYSTEM_B:
            config.verified |= PB_STATE_B_VERIFIED;
            config.remaining_boot_attempts = 0;
        break;
        case PBSTATE_SYSTEM_NONE:
        break;
        default:
            return -1;
        break;
    }
    return pbstate_commit();
}
