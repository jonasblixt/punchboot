/**
 * \file policy_chain.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The staggered chain boot source policy (boot.md 3.5): a list of
 * partitions, tried in the board's order, no state. It protects against an
 * image that doesn't load or verify, not against one that verifies and then
 * fails. 'boot activate' isn't supported, 'boot status' is the first of the
 * list.
 */

#ifndef INCLUDE_BOOT_POLICY_CHAIN_H
#define INCLUDE_BOOT_POLICY_CHAIN_H

#include <boot/policy.h>
#include <pb/utils_def.h>

extern const struct boot_policy_ops boot_policy_chain_ops;

/**
 * A board's boot source policy (board.boot_policy): the partitions with the
 * UUIDs in '_parts', in the order they are tried. '_parts' is an array of
 * const unsigned char *, at least one: the array itself, its size is taken.
 */
#define BOOT_POLICY_CHAIN(_parts)                                 \
    {                                                             \
        .ops = &boot_policy_chain_ops,                            \
        .config = PB_CONFIG(&(_parts)[0], const unsigned char *), \
        .length = ARRAY_SIZE(_parts) + PB_MUST_BE_ARRAY(_parts),  \
    }

#endif // INCLUDE_BOOT_POLICY_CHAIN_H
