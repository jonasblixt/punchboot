/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The core's calls to the board's boot source policy (boot.md 3).
 */

#include <boot/policy.h>
#include <pb/pb.h>

static const struct boot_policy *policy;

static const struct boot_policy_ops *policy_ops(void)
{
    return policy ? policy->ops : NULL;
}

void boot_policy_set(const struct boot_policy *policy_in)
{
    policy = policy_in;
}

int boot_policy_init(const struct boot_policy *policy_in)
{
    policy = policy_in;

    if (policy_ops() == NULL)
        return PB_OK;

    return policy->ops->init(policy);
}

int boot_policy_next(uuid_t part_uu)
{
    if (policy_ops() == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    return policy->ops->next(part_uu);
}

void boot_policy_report(const uuid_t part_uu, int result)
{
    if (policy_ops() != NULL)
        policy->ops->report(part_uu, result);
}

/* The policy's answer for the candidate it handed out last, not the boot's
 * mode, which the core keeps: see boot/policy.h */
enum boot_policy_mode boot_policy_mode(void)
{
    if (policy_ops() == NULL)
        return BOOT_POLICY_MODE_NORMAL;

    return policy->ops->mode();
}

int boot_policy_activate(uuid_t part_uu)
{
    if (policy_ops() == NULL)
        return -PB_ERR_NOT_SUPPORTED;

    return policy->ops->activate(part_uu);
}

void boot_policy_status(uuid_t part_uu)
{
    if (policy_ops() == NULL) {
        uuid_clear(part_uu);
        return;
    }

    policy->ops->status(part_uu);
}

/* Optional for a policy: one without systems has no names */
const char *boot_policy_system_name(uuid_t part_uu)
{
    if (policy_ops() == NULL || policy->ops->system_name == NULL)
        return NULL;

    return policy->ops->system_name(part_uu);
}
