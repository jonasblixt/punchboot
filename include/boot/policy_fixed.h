/**
 * \file policy_fixed.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The fixed boot source policy (boot.md 3.5): one partition, no state.
 * 'boot activate' isn't supported, 'boot status' is the partition.
 */

#ifndef INCLUDE_BOOT_POLICY_FIXED_H
#define INCLUDE_BOOT_POLICY_FIXED_H

#include <boot/policy.h>
#include <pb/utils_def.h>

extern const struct boot_policy_ops boot_policy_fixed_ops;

/**
 * A board's boot source policy (board.boot_policy): the one partition with
 * the UUID '_part', a const unsigned char *.
 */
#define BOOT_POLICY_FIXED(_part)                         \
    {                                                    \
        .ops = &boot_policy_fixed_ops,                   \
        .config = PB_CONFIG(_part, const unsigned char), \
    }

#endif // INCLUDE_BOOT_POLICY_FIXED_H
