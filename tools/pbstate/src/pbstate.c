
/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/pb_state_blob.h>
#include <errno.h>
#include <fcntl.h>
#include <pb/crc.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "pbstate.h"

static struct pb_boot_state state;
static const char *primary_device;
static const char *backup_device;

static pbstate_printfunc_t printfunc;

#define LOG(fmt, ...) \
    if (printfunc)    \
    printfunc(fmt, ##__VA_ARGS__)

static int verify_state(void)
{
    uint32_t crc = state.crc;
    int err = 0;

    state.crc = 0;

    if (state.magic != PB_STATE_MAGIC) {
        LOG("Error: Incorrect magic\n");
        err = -EIO;
        goto config_err_out;
    }

    if (crc != crc32(0, (uint8_t *)&state, sizeof(struct pb_boot_state))) {
        LOG("Error: CRC failed\n");
        err = -EIO;
        goto config_err_out;
    }

    /* TODO: Perform additional checks ? */

config_err_out:

    return err;
}

static int write_state(int fd)
{
    size_t write_sz = 0;
    const char *buffer = (const char *)&state;
    size_t buffer_len = sizeof(state);

    do {
        ssize_t write_now = write(fd, buffer + write_sz, buffer_len - write_sz);
        if (write_now == -1) {
            return -errno;
        }
        write_sz += write_now;
    } while (write_sz < buffer_len);

    return 0;
}

static int read_state(int fd)
{
    size_t read_sz = 0;
    char *buffer = (char *)&state;
    size_t buffer_len = sizeof(state);

    do {
        ssize_t read_now = read(fd, buffer + read_sz, buffer_len - read_sz);
        if (read_now == 0) {
            return -EIO;
        } else if (read_now == -1) {
            return -errno;
        }
        read_sz += read_now;
    } while (read_sz < buffer_len);

    return 0;
}

static int open_and_load_state(bool wr)
{
    int rc;
    int fd, backup_fd;

    fd = open(primary_device, wr ? (O_RDWR | O_DSYNC) : O_RDONLY);

    if (fd == -1)
        return -errno;

    // We only take a lock on the primary device fd. The library shall ensure
    // that we don't interact with the backup device if we can't get a lock
    // on the primary device.
    do {
        rc = flock(fd, wr ? LOCK_EX : LOCK_SH);
    } while (rc == -1 && errno == EINTR);

    if (rc != 0) {
        rc = -errno;
        goto err_close_out;
    }

    rc = read_state(fd);

    if (rc != 0) {
        goto err_close_and_release_out;
    }

    rc = verify_state();

    // If the primary state is okay, we just return the fd and won't check
    // the backup state since it will be overwritten anyway.
    if (rc == 0)
        return fd;

    // If the primary state is corruptet we should try to load the
    // backup.
    backup_fd = open(backup_device, wr ? O_RDWR : O_RDONLY);

    if (backup_fd == -1) {
        rc = -errno;
        goto err_close_and_release_out;
    }

    rc = read_state(backup_fd);

    if (rc != 0) {
        goto err_close_backup_out;
    }

    rc = verify_state();

    if (rc != 0) {
        goto err_close_backup_out;
    }

    close(backup_fd);
    return fd;
err_close_backup_out:
    close(backup_fd);
err_close_and_release_out:
    flock(fd, LOCK_UN);
err_close_out:
    close(fd);
    return rc;
}

static int close_and_save_state(int fd, bool wr)
{
    int rc = 0;
    int backup_fd;
    uint32_t crc;

    if (!wr) {
        goto err_close_and_release_out;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        rc = -errno;
        goto err_close_and_release_out;
    }

    state.crc = 0;
    crc = crc32(0, (const uint8_t *)&state, sizeof(struct pb_boot_state));
    state.crc = crc;

    rc = write_state(fd);

    if (rc != 0) {
        LOG("Could not write primary config data (%i)\n", rc);
        goto err_close_and_release_out;
    }

    backup_fd = open(backup_device, O_WRONLY | O_DSYNC);

    if (backup_fd == -1) {
        rc = -errno;
        LOG("Error: Could not open '%s' (%i)\n", backup_device, rc);
        goto err_close_and_release_out;
    }

    /* Synchronize backup partition with primary partition */
    rc = write_state(backup_fd);

    if (rc != 0) {
        LOG("Could not write backup config data (%i)\n", rc);
        goto err_close_backup_out;
    }

err_close_backup_out:
    close(backup_fd);
err_close_and_release_out:
    flock(fd, LOCK_UN);
    close(fd);
    sync(); /* Ensure that data is written to disk */
    return rc;
}

int pbstate_init(const char *p_device, const char *b_device, pbstate_printfunc_t _printfunc)
{
    primary_device = p_device;
    backup_device = b_device;
    printfunc = _printfunc;

    return 0;
}

int pbstate_is_system_active(pbstate_system_t system)
{
    int rc;
    int result = 0;
    int fd = open_and_load_state(false);

    if (fd < 0)
        return fd;

    switch (system) {
    case PBSTATE_SYSTEM_A:
        result = !!(state.enable & PB_STATE_A_ENABLED);
        break;
    case PBSTATE_SYSTEM_B:
        result = !!(state.enable & PB_STATE_B_ENABLED);
        break;
    case PBSTATE_SYSTEM_NONE:
        result = !!(state.enable);
        break;
    default:
        break;
    }

    rc = close_and_save_state(fd, false);

    if (rc < 0)
        return rc;

    return result;
}

int pbstate_is_system_verified(pbstate_system_t system)
{
    int rc;
    int result = 0;
    int fd = open_and_load_state(false);

    if (fd < 0)
        return fd;

    switch (system) {
    case PBSTATE_SYSTEM_A:
        result = !!(state.verified & PB_STATE_A_VERIFIED);
        break;
    case PBSTATE_SYSTEM_B:
        result = !!(state.verified & PB_STATE_B_VERIFIED);
        break;
    case PBSTATE_SYSTEM_NONE:
        result = !!(state.enable);
        break;
    default:
        break;
    }

    rc = close_and_save_state(fd, false);

    if (rc < 0)
        return rc;

    return result;
}

int pbstate_get_remaining_boot_attempts(unsigned int *boot_attempts)
{
    if (boot_attempts == NULL)
        return -EINVAL;

    int fd = open_and_load_state(false);

    if (fd < 0)
        return fd;

    (*boot_attempts) = state.remaining_boot_attempts;

    return close_and_save_state(fd, false);
}

int pbstate_force_rollback(void)
{
    int rc;
    int preserved_rc = 0;
    int fd = open_and_load_state(true);

    if (fd < 0)
        return fd;

    /* Rolling back a verified system is not allowed */
    if ((state.enable & PB_STATE_A_ENABLED) && (state.verified & PB_STATE_A_VERIFIED)) {
        rc = -EPERM;
        goto err_close_release_no_save_out;
    }

    if ((state.enable & PB_STATE_B_ENABLED) && (state.verified & PB_STATE_B_VERIFIED)) {
        rc = -EPERM;
        goto err_close_release_no_save_out;
    }

    state.remaining_boot_attempts = 0;

    rc = close_and_save_state(fd, true);

    if (rc < 0)
        return rc;

    return 0;

err_close_release_no_save_out:
    preserved_rc = rc;
    rc = close_and_save_state(fd, false);

    if (rc < 0)
        return rc;

    return preserved_rc;
}

int pbstate_get_errors(uint32_t *error)
{
    int fd = open_and_load_state(false);

    if (fd < 0)
        return fd;

    (*error) = state.error;

    return close_and_save_state(fd, false);
}

int pbstate_clear_error(uint32_t mask)
{
    int fd = open_and_load_state(true);

    if (fd < 0)
        return fd;

    state.error &= ~mask;

    return close_and_save_state(fd, true);
}

int pbstate_switch_system(pbstate_system_t system, uint32_t boot_attempts)
{
    int rc;
    int preserved_rc;
    int fd = open_and_load_state(true);

    if (fd < 0)
        return fd;

    switch (system) {
    case PBSTATE_SYSTEM_A:
        state.enable = PB_STATE_A_ENABLED;

        if (boot_attempts > 0) {
            state.remaining_boot_attempts = boot_attempts;
            state.verified &= ~PB_STATE_A_VERIFIED;
            state.error = 0;
        } else {
            state.verified |= PB_STATE_A_VERIFIED;
            state.remaining_boot_attempts = 0;
            state.error = 0;
        }
        break;
    case PBSTATE_SYSTEM_B:
        state.enable = PB_STATE_B_ENABLED;

        if (boot_attempts > 0) {
            state.remaining_boot_attempts = boot_attempts;
            state.verified &= ~PB_STATE_B_VERIFIED;
            state.error = 0;
        } else {
            state.verified |= PB_STATE_B_VERIFIED;
            state.remaining_boot_attempts = 0;
            state.error = 0;
        }
        break;
    case PBSTATE_SYSTEM_NONE:
        state.enable = 0;
        state.remaining_boot_attempts = 0;
        break;
    default:
        rc = -EINVAL;
        goto err_close_release_no_save_out;
        break;
    }

    return close_and_save_state(fd, true);

err_close_release_no_save_out:
    preserved_rc = rc;

    rc = close_and_save_state(fd, false);

    if (rc < 0)
        return rc;

    return preserved_rc;
}

int pbstate_invalidate_system(pbstate_system_t system)
{
    int rc;
    int preserved_rc;
    int fd = open_and_load_state(true);

    if (fd < 0)
        return fd;

    switch (system) {
    case PBSTATE_SYSTEM_A:
        state.enable &= ~PB_STATE_A_ENABLED;
        state.verified &= ~PB_STATE_A_VERIFIED;
        break;
    case PBSTATE_SYSTEM_B:
        state.enable &= ~PB_STATE_B_ENABLED;
        state.verified &= ~PB_STATE_B_VERIFIED;
        break;
    default:
        rc = -EINVAL;
        goto err_close_release_no_save_out;
        break;
    }

    return close_and_save_state(fd, true);

err_close_release_no_save_out:
    preserved_rc = rc;

    rc = close_and_save_state(fd, false);

    if (rc < 0)
        return rc;

    return preserved_rc;
}

int pbstate_set_system_verified(pbstate_system_t system)
{
    int rc;
    int preserved_rc;
    int fd = open_and_load_state(true);

    if (fd < 0)
        return fd;

    switch (system) {
    case PBSTATE_SYSTEM_A:
        state.verified |= PB_STATE_A_VERIFIED;
        state.remaining_boot_attempts = 0;
        break;
    case PBSTATE_SYSTEM_B:
        state.verified |= PB_STATE_B_VERIFIED;
        state.remaining_boot_attempts = 0;
        break;
    case PBSTATE_SYSTEM_NONE:
        break;
    default:
        rc = -EINVAL;
        goto err_close_release_no_save_out;
        break;
    }

    return close_and_save_state(fd, true);

err_close_release_no_save_out:
    preserved_rc = rc;

    rc = close_and_save_state(fd, false);

    if (rc < 0)
        return rc;

    return preserved_rc;
}

int pbstate_read_board_reg(unsigned int index, uint32_t *value)
{
    if (index > (PB_STATE_NO_OF_BOARD_REGS - 1))
        return -EINVAL;

    int fd = open_and_load_state(false);

    if (fd < 0)
        return fd;

    *value = state.board_regs[PB_STATE_NO_OF_BOARD_REGS - index - 1];

    return close_and_save_state(fd, false);
}

int pbstate_write_board_reg(unsigned int index, uint32_t value)
{
    if (index > (PB_STATE_NO_OF_BOARD_REGS - 1))
        return -EINVAL;

    int fd = open_and_load_state(true);

    if (fd < 0)
        return fd;

    state.board_regs[PB_STATE_NO_OF_BOARD_REGS - index - 1] = value;

    return close_and_save_state(fd, true);
}

int pbstate_clear_set_board_reg(unsigned int index, uint32_t clear, uint32_t set)
{
    if (index > (PB_STATE_NO_OF_BOARD_REGS - 1))
        return -EINVAL;

    int fd = open_and_load_state(true);

    if (fd < 0)
        return fd;

    state.board_regs[PB_STATE_NO_OF_BOARD_REGS - index - 1] &= ~clear;
    state.board_regs[PB_STATE_NO_OF_BOARD_REGS - index - 1] |= set;

    return close_and_save_state(fd, true);
}
