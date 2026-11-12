/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The fixed boot source policy (boot.md 3.5): one partition, no state.
 */

#include <boot/policy_fixed.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdbool.h>

static const unsigned char *part; /* NULL: not set up */
static bool handed_out;

static int fixed_init(const struct boot_policy *policy)
{
    bio_dev_t dev;
    int rc;

    part = NULL;
    handed_out = false;

    if (policy->config == NULL)
        return -PB_ERR_PARAM;

    dev = bio_get_part_by_uu(policy->config);

    if (dev < 0) {
        LOG_ERR("The boot partition isn't in the table (%i)", dev);
        return dev;
    }

    rc = bio_clear_set_flags(dev, 0, BIO_FLAG_BOOTABLE);

    if (rc != PB_OK)
        return rc;

    part = policy->config;
    return PB_OK;
}

static int fixed_next(uuid_t part_uu)
{
    if (part == NULL)
        return -PB_ERR_STATE;

    if (handed_out) {
        handed_out = false;
        return -PB_ERR_NOT_FOUND;
    }

    handed_out = true;
    uuid_copy(part_uu, part);
    return PB_OK;
}

static void fixed_report(const uuid_t part_uu, int result)
{
    (void)part_uu;

    /* The boot goes on with it: the next boot starts over */
    if (result == PB_OK)
        handed_out = false;
}

static enum boot_policy_mode fixed_mode(void)
{
    return BOOT_POLICY_MODE_NORMAL;
}

static int fixed_activate(uuid_t part_uu)
{
    (void)part_uu;
    return -PB_ERR_NOT_SUPPORTED;
}

static void fixed_status(uuid_t part_uu)
{
    if (part == NULL)
        uuid_clear(part_uu);
    else
        uuid_copy(part_uu, part);
}

const struct boot_policy_ops boot_policy_fixed_ops = {
    .init = fixed_init,
    .next = fixed_next,
    .report = fixed_report,
    .mode = fixed_mode,
    .activate = fixed_activate,
    .status = fixed_status,
};
