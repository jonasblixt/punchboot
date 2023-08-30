/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <uuid.h>
#include <pb/pb.h>
#include <pb/crc.h>
#include <pb/bio.h>
#include <boot/boot.h>
#include <boot/ab_state.h>
#include <boot/pb_state_blob.h>

static bio_dev_t primary_part, backup_part;
static struct pb_boot_state boot_state, boot_state_backup;
static const struct boot_ab_state_config *cfg;

static int validate(struct pb_boot_state *state)
{
    uint32_t crc = state->crc;
    int err = PB_OK;

    state->crc = 0;

    if (state->magic != PB_STATE_MAGIC) {
        LOG_ERR("Incorrect magic");
        err = -PB_ERR;
        goto config_err_out;
    }

    if (crc != crc32(0, (uint8_t *) state, sizeof(struct pb_boot_state))) {
        LOG_ERR("CRC failed");
        err =- PB_ERR;
        goto config_err_out;
    }

config_err_out:
    return err;
}

static int pb_boot_state_defaults(struct pb_boot_state *state)
{
    memset(state, 0, sizeof(struct pb_boot_state));
    state->magic = PB_STATE_MAGIC;
    return PB_OK;
}

static int pb_boot_state_commit(void)
{
    int rc;

    boot_state.crc = 0;
    uint32_t crc = crc32(0, (const uint8_t *) &boot_state,
                                sizeof(struct pb_boot_state));
    boot_state.crc = crc;

    rc = bio_write(primary_part, 0, sizeof(struct pb_boot_state), &boot_state);
    if (rc != PB_OK)
        goto config_commit_err;

    rc = bio_write(backup_part, 0, sizeof(struct pb_boot_state), &boot_state);

config_commit_err:
    if (rc != PB_OK) {
        LOG_ERR("Could not write boot state");
    } else {
        LOG_INFO("Boot state written");
    }

    return rc;
}

int boot_ab_state_init(const struct boot_ab_state_config *state_cfg)
{
    int rc;
    bio_dev_t dev;
    bool primary_state_ok = false;
    bool backup_state_ok = false;

    cfg = state_cfg;

    dev = bio_get_part_by_uu(cfg->sys_a_uu);

    if (dev < 0)
        return dev;

    bio_clear_set_flags(dev, 0, BIO_FLAG_BOOTABLE);

    dev = bio_get_part_by_uu(cfg->sys_b_uu);

    if (dev < 0)
        return dev;

    bio_clear_set_flags(dev, 0, BIO_FLAG_BOOTABLE);

    primary_part = bio_get_part_by_uu(cfg->primary_state_part_uu);

    if (primary_part < 0) {
        LOG_ERR("Primary boot state partition not found");
    }

    backup_part = bio_get_part_by_uu(cfg->backup_state_part_uu);

    if (backup_part < 0) {
        LOG_ERR("Backup boot state partition not found");
    }

    (void) bio_read(primary_part, 0, sizeof(struct pb_boot_state), &boot_state);

    rc = validate(&boot_state);

    if (rc == PB_OK) {
        primary_state_ok = true;
    } else {
        LOG_ERR("Primary boot state data corrupt");
        primary_state_ok = false;
    }

    (void) bio_read(backup_part, 0, sizeof(struct pb_boot_state), &boot_state_backup);

    rc = validate(&boot_state_backup);

    if (rc == PB_OK) {
        backup_state_ok = true;
    } else {
        LOG_ERR("Backup boot state data corrupt");
        backup_state_ok = false;
    }

    if (!primary_state_ok && !backup_state_ok) {
        LOG_ERR("No valid state found, installing default");
        pb_boot_state_defaults(&boot_state);
        pb_boot_state_defaults(&boot_state_backup);
        rc = pb_boot_state_commit();
    } else if (!backup_state_ok && primary_state_ok) {
        LOG_ERR("Backup state corrupt, repairing");
        pb_boot_state_defaults(&boot_state_backup);
        rc = pb_boot_state_commit();
    } else if (backup_state_ok && !primary_state_ok) {
        LOG_ERR("Primary state corrupt, reparing");
        memcpy(&boot_state, &boot_state_backup, sizeof(struct pb_boot_state));
        rc = pb_boot_state_commit();
    } else {
        LOG_INFO("Boot state loaded");
        rc = PB_OK;
    }

    return rc;
}

bio_dev_t boot_ab_state_get(void)
{
    int rc;
    const unsigned char *boot_part_uu = NULL;
    bool commit = false;

    LOG_DBG("A/B boot load state %u %u %u", boot_state.enable,
                                            boot_state.verified,
                                            boot_state.error);

    if (boot_state.enable & PB_STATE_A_ENABLED) {
        if (!(boot_state.verified & PB_STATE_A_VERIFIED) &&
             boot_state.remaining_boot_attempts > 0) {
            boot_state.remaining_boot_attempts--;
            commit = true; /* Update state data */
            boot_part_uu = cfg->sys_a_uu;
        } else if (!(boot_state.verified & PB_STATE_A_VERIFIED)) {
            LOG_ERR("Rollback to B system");
            if (!(boot_state.verified & PB_STATE_B_VERIFIED)) {
                if (cfg->rollback_mode == AB_ROLLBACK_MODE_SPECULATIVE) {
                    // Enable B
                    // Reset boot counter to one
                    commit = true;
                    boot_state.enable = PB_STATE_B_ENABLED;
                    boot_state.remaining_boot_attempts = 1;
                    boot_state.error = PB_STATE_ERROR_A_ROLLBACK;
                    boot_part_uu = cfg->sys_b_uu;
                } else {
                    LOG_ERR("B system not verified, failing");
                    return -PB_ERR;
                }
            } else {
                commit = true;
                boot_state.enable = PB_STATE_B_ENABLED;
                boot_state.error = PB_STATE_ERROR_A_ROLLBACK;
                boot_part_uu = cfg->sys_b_uu;
            }
        } else {
            boot_part_uu = cfg->sys_a_uu;
        }
    } else if (boot_state.enable & PB_STATE_B_ENABLED) {
        if (!(boot_state.verified & PB_STATE_B_VERIFIED) &&
             boot_state.remaining_boot_attempts > 0) {
            boot_state.remaining_boot_attempts--;
            commit = true; /* Update state data */
            boot_part_uu = cfg->sys_b_uu;
        } else if (!(boot_state.verified & PB_STATE_B_VERIFIED)) {
            LOG_ERR("Rollback to A system");
            if (!(boot_state.verified & PB_STATE_A_VERIFIED)) {
                if (cfg->rollback_mode == AB_ROLLBACK_MODE_SPECULATIVE) {
                    // Enable A
                    // Reset boot counter to one
                    commit = true;
                    boot_state.enable = PB_STATE_A_ENABLED;
                    boot_state.remaining_boot_attempts = 1;
                    boot_state.error = PB_STATE_ERROR_B_ROLLBACK;
                    boot_part_uu = cfg->sys_a_uu;
                } else {
                    LOG_ERR("A system not verified, failing");
                    return -PB_ERR;
                }
            }

            commit = true;
            boot_state.enable = PB_STATE_A_ENABLED;
            boot_state.error = PB_STATE_ERROR_B_ROLLBACK;
            boot_part_uu = cfg->sys_a_uu;
        } else {
            boot_part_uu = cfg->sys_b_uu;
        }
    } else {
        boot_part_uu = NULL;
    }

    if (commit) {
        rc = pb_boot_state_commit();

        if (rc != PB_OK)
            return rc;
    }

    if (boot_part_uu == NULL)
        return -PB_ERR_NO_ACTIVE_BOOT_PARTITION;

    return bio_get_part_by_uu(boot_part_uu);
}

int boot_ab_state_set_boot_partition(uuid_t part_uu)
{
    if (uuid_compare(part_uu, cfg->sys_a_uu) == 0) {
        boot_state.enable = PB_STATE_A_ENABLED;
        boot_state.verified = PB_STATE_A_VERIFIED;
        boot_state.error = 0;
    } else if (uuid_compare(part_uu, cfg->sys_b_uu) == 0) {
        boot_state.enable = PB_STATE_B_ENABLED;
        boot_state.verified = PB_STATE_B_VERIFIED;
        boot_state.error = 0;
    } else if (uuid_is_null(part_uu)) {
        boot_state.enable = 0;
        boot_state.verified = 0;
        boot_state.error = 0;
    } else {
        boot_state.enable = 0;
        boot_state.verified = 0;
        boot_state.error = 0;
        return -PB_ERR_PART_NOT_BOOTABLE;
    }

    return pb_boot_state_commit();
}

void boot_ab_state_get_boot_partition(uuid_t part_uu)
{
    switch (boot_state.enable) {
        case PB_STATE_A_ENABLED:
            uuid_copy(part_uu, cfg->sys_a_uu);
        break;
        case PB_STATE_B_ENABLED:
            uuid_copy(part_uu, cfg->sys_b_uu);
        break;
        default:
            uuid_clear(part_uu);
    }
}

const char *boot_ab_part_uu_to_name(uuid_t part_uu)
{
    if (uuid_compare(part_uu, cfg->sys_a_uu) == 0) {
        return "A";
    } else if (uuid_compare(part_uu, cfg->sys_b_uu) == 0) {
        return "B";
    } else {
        return "?";
    }
}

int boot_ab_state_read_board_reg(unsigned int index, uint32_t *value)
{
    if (index > (PB_STATE_NO_OF_BOARD_REGS - 1))
        return -PB_ERR_PARAM;
    (*value) = boot_state.board_regs[PB_STATE_NO_OF_BOARD_REGS - index - 1];
    return 0;
}

int boot_ab_state_write_board_reg(unsigned int index, uint32_t value)
{
    if (index > (PB_STATE_NO_OF_BOARD_REGS - 1))
        return -PB_ERR_PARAM;
    boot_state.board_regs[PB_STATE_NO_OF_BOARD_REGS - index - 1] = value;
    return pb_boot_state_commit();
}
