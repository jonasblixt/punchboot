/**
 * \file policy.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A board's boot source policy (board.boot_policy, boot.md 3): the policy's
 * functions and the board's configuration for it, made by the policy's
 * macro: BOOT_POLICY_AB() (boot/ab_state.h).
 */

#ifndef INCLUDE_BOOT_POLICY_H
#define INCLUDE_BOOT_POLICY_H

#include <pb/bio.h>
#include <uuid.h>

struct boot_policy_ops {
    int (*init)(const void *config);
    bio_dev_t (*get)(void); /*!< The partition to boot */
    int (*activate)(uuid_t part_uu); /*!< A host activates a partition */
    void (*status)(uuid_t part_uu); /*!< The active partition, or all zero */
};

struct boot_policy {
    const struct boot_policy_ops *ops; /*!< NULL: no boot from storage */
    const void *config;
};

#endif // INCLUDE_BOOT_POLICY_H
