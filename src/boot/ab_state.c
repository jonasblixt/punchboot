/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/ab_state.h>
#include <boot/boot.h>
#include <boot/pb_state_blob.h>
#include <pb/bio.h>
#include <pb/crc.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>

static bio_dev_t primary_part, backup_part;
static struct pb_boot_state boot_state, boot_state_backup;
static const struct boot_ab_state_config *cfg;
/* This boot's candidates: how many were handed out, and the first one */
static unsigned int handed_out;
static const unsigned char *first_slot;
static enum boot_policy_mode mode;

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

    if (crc != crc32(0, (uint8_t *)state, sizeof(struct pb_boot_state))) {
        LOG_ERR("CRC failed");
        err = -PB_ERR;
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

/* NOR flash only clears bits when it is written: erase the block first */
static int pb_boot_state_write(bio_dev_t part)
{
    if (bio_get_flags(part) & BIO_FLAG_ERASE_BEFORE_WRITE) {
        int rc = bio_erase(part, 0, 1);

        if (rc != PB_OK)
            return rc;
    }

    return bio_write(part, 0, sizeof(struct pb_boot_state), &boot_state);
}

static int pb_boot_state_commit(void)
{
    int rc;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    boot_state.crc = 0;
    uint32_t crc = crc32(0, (const uint8_t *)&boot_state, sizeof(struct pb_boot_state));
    boot_state.crc = crc;

    rc = pb_boot_state_write(primary_part);
    if (rc != PB_OK)
        goto config_commit_err;

    rc = pb_boot_state_write(backup_part);

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
    handed_out = 0;
    first_slot = NULL;
    mode = BOOT_POLICY_MODE_NORMAL;

    if (cfg == NULL)
        return -PB_ERR_PARAM;

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

    (void)bio_read(primary_part, 0, sizeof(struct pb_boot_state), &boot_state);

    rc = validate(&boot_state);

    if (rc == PB_OK) {
        primary_state_ok = true;
    } else {
        LOG_ERR("Primary boot state data corrupt");
        primary_state_ok = false;
    }

    (void)bio_read(backup_part, 0, sizeof(struct pb_boot_state), &boot_state_backup);

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

/*
 * boot.md 3.6: Kconfig is speculative mode's only switch, and it carries the
 * watchdog dependency. A constant, so that both builds compile all of it.
 */
#ifdef CONFIG_BOOT_AB_SPECULATIVE
static const bool speculative = true;
#else
static const bool speculative = false;
#endif

/*
 * Speculative mode's first candidate (boot.md 3.6, rules 1, 3 and 4): A/B has
 * no viable candidate by its state, so 'slot' is enabled, and the caller
 * writes that before the load. The next boot finds it enabled, not confirmed
 * and without attempts, and enables the other one: a hang and the watchdog
 * lead there. Nothing else of the state changes, but a counter that was left
 * behind with nothing enabled: no attempts are counted.
 */
static void speculative_enable(uint32_t slot)
{
    boot_state.enable = slot;
    boot_state.remaining_boot_attempts = 0;
}

/*
 * The first candidate of a boot (boot.md 3.5, rules 1 and 2): the enabled
 * slot when it is verified or on trial with attempts left, else the slot it
 * rolls back to. With speculative mode a boot without such a candidate has
 * one (boot.md 3.6): the other slot, or A when nothing is enabled. What the
 * decision changes is written before it returns.
 */
static int first_candidate(const unsigned char **part_uu, enum boot_policy_mode *how)
{
    int rc;
    const unsigned char *boot_part_uu = NULL;
    bool commit = false;

    LOG_DBG(
        "A/B boot load state %u %u %u", boot_state.enable, boot_state.verified, boot_state.error);

    *how = BOOT_POLICY_MODE_NORMAL;

    if (boot_state.enable & PB_STATE_A_ENABLED) {
        if (!(boot_state.verified & PB_STATE_A_VERIFIED) &&
            boot_state.remaining_boot_attempts > 0) {
            boot_state.remaining_boot_attempts--;
            commit = true; /* Update state data */
            boot_part_uu = cfg->sys_a_uu;
        } else if (!(boot_state.verified & PB_STATE_A_VERIFIED)) {
            LOG_ERR("Rollback to B system");
            if (!(boot_state.verified & PB_STATE_B_VERIFIED)) {
                if (!speculative) {
                    LOG_ERR("B system not verified, failing");
                    return -PB_ERR_NOT_FOUND;
                }

                LOG_ERR("B system not verified: speculative mode");
                speculative_enable(PB_STATE_B_ENABLED);
                commit = true;
                *how = BOOT_POLICY_MODE_SPECULATIVE;
                boot_part_uu = cfg->sys_b_uu;
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
                if (!speculative) {
                    LOG_ERR("A system not verified, failing");
                    return -PB_ERR_NOT_FOUND;
                }

                LOG_ERR("A system not verified: speculative mode");
                speculative_enable(PB_STATE_A_ENABLED);
                commit = true;
                *how = BOOT_POLICY_MODE_SPECULATIVE;
                boot_part_uu = cfg->sys_a_uu;
            } else {
                commit = true;
                boot_state.enable = PB_STATE_A_ENABLED;
                boot_state.error = PB_STATE_ERROR_B_ROLLBACK;
                boot_part_uu = cfg->sys_a_uu;
            }
        } else {
            boot_part_uu = cfg->sys_b_uu;
        }
    } else if (speculative) {
        /* Nothing enabled: A first (boot.md 3.6, rule 1, D-040) */
        LOG_ERR("No system enabled: speculative mode");
        speculative_enable(PB_STATE_A_ENABLED);
        commit = true;
        *how = BOOT_POLICY_MODE_SPECULATIVE;
        boot_part_uu = cfg->sys_a_uu;
    } else {
        boot_part_uu = NULL;
    }

    if (commit) {
        rc = pb_boot_state_commit();

        if (rc != PB_OK)
            return rc;
    }

    if (boot_part_uu == NULL)
        return -PB_ERR_NOT_FOUND;

    *part_uu = boot_part_uu;
    return PB_OK;
}

/*
 * The candidate after the first one failed in this boot: the other slot,
 * for this boot only. When it is verified: boot.md 3.5, rule 3, mode
 * fallback. When it isn't, there is none, but with speculative mode, which
 * tries it (boot.md 3.6, rule 1's second case and rule 3). Nothing is
 * written either way: the next boot starts from the state again.
 */
static int fallback_candidate(const unsigned char **part_uu, enum boot_policy_mode *how)
{
    const unsigned char *other;
    uint32_t other_verified;

    if (first_slot == cfg->sys_a_uu) {
        other = cfg->sys_b_uu;
        other_verified = PB_STATE_B_VERIFIED;
    } else if (first_slot == cfg->sys_b_uu) {
        other = cfg->sys_a_uu;
        other_verified = PB_STATE_A_VERIFIED;
    } else {
        return -PB_ERR_NOT_FOUND;
    }

    if (boot_state.verified & other_verified)
        *how = BOOT_POLICY_MODE_FALLBACK;
    else if (speculative)
        *how = BOOT_POLICY_MODE_SPECULATIVE;
    else
        return -PB_ERR_NOT_FOUND;

    *part_uu = other;
    return PB_OK;
}

static int policy_ab_next(uuid_t part_uu)
{
    const unsigned char *candidate = NULL;
    enum boot_policy_mode how = BOOT_POLICY_MODE_NORMAL;
    int rc;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    switch (handed_out) {
    case 0:
        rc = first_candidate(&candidate, &how);
        first_slot = candidate;
        mode = BOOT_POLICY_MODE_NORMAL;
        break;
    case 1:
        rc = fallback_candidate(&candidate, &how);
        if (rc == PB_OK) {
            LOG_ERR("System %s failed, trying %s for this boot",
                    boot_ab_part_uu_to_name((unsigned char *)first_slot),
                    boot_ab_part_uu_to_name((unsigned char *)candidate));
        }
        break;
    default:
        rc = -PB_ERR_NOT_FOUND;
    }

    if (rc != PB_OK) {
        /* This boot has no more: the next one starts over */
        handed_out = 0;
        return rc;
    }

    mode = how;
    handed_out++;
    uuid_copy(part_uu, candidate);
    return PB_OK;
}

static void policy_ab_report(const uuid_t part_uu, int result)
{
    (void)part_uu;

    /* Nothing is persisted on a failure. The boot goes on with a candidate
     * that succeeded: the next boot starts over. The mode stays, the
     * handover reads it */
    if (result == PB_OK)
        handed_out = 0;
}

static enum boot_policy_mode policy_ab_mode(void)
{
    return mode;
}

int boot_ab_state_set_boot_partition(uuid_t part_uu)
{
    if (cfg == NULL)
        return -PB_ERR_STATE;

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
    if (cfg == NULL) {
        uuid_clear(part_uu);
        return;
    }

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
    if (cfg == NULL)
        return "?";

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

static int policy_ab_init(const struct boot_policy *policy)
{
    return boot_ab_state_init(policy->config);
}

const struct boot_policy_ops boot_policy_ab_ops = {
    .init = policy_ab_init,
    .next = policy_ab_next,
    .report = policy_ab_report,
    .mode = policy_ab_mode,
    .activate = boot_ab_state_set_boot_partition,
    .status = boot_ab_state_get_boot_partition,
};
