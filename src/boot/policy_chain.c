/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The staggered chain boot source policy (boot.md 3.5): a list of
 * partitions, tried in the board's order, no state.
 */

#include <boot/policy_chain.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdbool.h>

static const unsigned char *const *parts; /* NULL: not set up */
static size_t no_of_parts;
static size_t next_part; /* The next one this boot hands out */
static bool fallback; /* The last one handed out wasn't the first */

static int chain_init(const struct boot_policy *policy)
{
    const unsigned char *const *list = policy->config;

    parts = NULL;
    next_part = 0;
    fallback = false;

    if (list == NULL || policy->length == 0)
        return -PB_ERR_PARAM;

    for (size_t i = 0; i < policy->length; i++) {
        bio_dev_t dev;
        int rc;

        if (list[i] == NULL)
            return -PB_ERR_PARAM;

        dev = bio_get_part_by_uu(list[i]);

        if (dev < 0) {
            LOG_ERR("Boot partition %zu isn't in the table (%i)", i, dev);
            return dev;
        }

        rc = bio_clear_set_flags(dev, 0, BIO_FLAG_BOOTABLE);

        if (rc != PB_OK)
            return rc;
    }

    parts = list;
    no_of_parts = policy->length;
    return PB_OK;
}

static int chain_next(uuid_t part_uu)
{
    if (parts == NULL)
        return -PB_ERR_STATE;

    if (next_part >= no_of_parts) {
        next_part = 0;
        return -PB_ERR_NOT_FOUND;
    }

    fallback = (next_part > 0);
    uuid_copy(part_uu, parts[next_part++]);
    return PB_OK;
}

static void chain_report(const uuid_t part_uu, int result)
{
    (void)part_uu;

    /* The boot goes on with it: the next boot starts over. The mode stays,
     * the handover reads it */
    if (result == PB_OK)
        next_part = 0;
}

static enum boot_policy_mode chain_mode(void)
{
    return fallback ? BOOT_POLICY_MODE_FALLBACK : BOOT_POLICY_MODE_NORMAL;
}

static int chain_activate(uuid_t part_uu)
{
    (void)part_uu;
    return -PB_ERR_NOT_SUPPORTED;
}

static void chain_status(uuid_t part_uu)
{
    if (parts == NULL)
        uuid_clear(part_uu);
    else
        uuid_copy(part_uu, parts[0]);
}

const struct boot_policy_ops boot_policy_chain_ops = {
    .init = chain_init,
    .next = chain_next,
    .report = chain_report,
    .mode = chain_mode,
    .activate = chain_activate,
    .status = chain_status,
};
