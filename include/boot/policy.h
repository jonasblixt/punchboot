/**
 * \file policy.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The boot source policy (boot.md 3): which partitions a boot tries, in
 * which order, and the state that decides it. One per board:
 * board.boot_policy is the object of the policy the board chose, the
 * policy's functions and the board's data for it, made by the policy's
 * macro: BOOT_POLICY_FIXED() (boot/policy_fixed.h), BOOT_POLICY_CHAIN()
 * (boot/policy_chain.h), BOOT_POLICY_AB() (boot/ab_state.h).
 *
 * The core calls the board's policy through boot_policy_*() below. A policy
 * implements the functions of struct boot_policy_ops.
 */

#ifndef INCLUDE_BOOT_POLICY_H
#define INCLUDE_BOOT_POLICY_H

#include <stddef.h>
#include <uuid.h>

/**
 * How a boot was chosen, for the handover (boot.md 5.6). The values are
 * boot_mode's in pb's structure.
 */
enum boot_policy_mode {
    BOOT_POLICY_MODE_NORMAL = 0, /*!< The first candidate of the boot */
    BOOT_POLICY_MODE_FALLBACK = 1, /*!< Another one, the first failed */
    BOOT_POLICY_MODE_SPECULATIVE = 2, /*!< A/B's speculative mode */
};

struct boot_policy;

struct boot_policy_ops {
    /**
     * Reads and repairs the policy's state and marks its candidates
     * bootable (BIO_FLAG_BOOTABLE). It can run more than once.
     */
    int (*init)(const struct boot_policy *policy);

    /**
     * The next candidate of this boot. A policy with state writes what the
     * decision changes before it returns. A boot's candidates start over
     * after a candidate was reported with PB_OK and after 'next' returned
     * an error.
     *
     * @return PB_OK with a candidate in 'part_uu',
     *        -PB_ERR_NOT_FOUND when none is left
     */
    int (*next)(uuid_t part_uu);

    /** The result of the attempt with the candidate 'part_uu' */
    void (*report)(const uuid_t part_uu, int result);

    /** How the candidate that 'next' handed out last was chosen */
    enum boot_policy_mode (*mode)(void);

    /**
     * Command mode's 'boot activate'
     *
     * @return -PB_ERR_NOT_SUPPORTED where the policy has nothing to activate
     */
    int (*activate)(uuid_t part_uu);

    /** Command mode's 'boot status': the partition, or the null UUID */
    void (*status)(uuid_t part_uu);
};

struct boot_policy {
    const struct boot_policy_ops *ops; /*!< NULL: no boot from storage */
    const void *config; /*!< The policy's: its configuration or its list */
    size_t length; /*!< A list: the number of partitions */
};

/**
 * The policy the functions below call: the board's. boot_init() sets it,
 * and boot_policy_init() does.
 *
 * @param[in] policy The board's policy. NULL, or one without ops: none
 */
void boot_policy_set(const struct boot_policy *policy);

/**
 * Sets the board's policy up (boot.md 3.1). Called by board_setup(), after
 * the partition table.
 *
 * @param[in] policy The board's data for the policy
 *
 * @return PB_OK, also without a policy, or the policy's error
 */
int boot_policy_init(const struct boot_policy *policy);

/**
 * The next candidate of this boot (boot.md 3.2)
 *
 * @param[out] part_uu The candidate's partition
 *
 * @return PB_OK with a candidate,
 *        -PB_ERR_NOT_FOUND when none is left,
 *        -PB_ERR_NOT_SUPPORTED without a policy,
 *         or the error of a state write that failed
 */
int boot_policy_next(uuid_t part_uu);

/** The result of the attempt with 'part_uu' (boot.md 3.3) */
void boot_policy_report(const uuid_t part_uu, int result);

/**
 * How the candidate the policy handed out last was chosen (boot.md 3.4)
 *
 * This is the policy's answer, not the boot's: a boot of a partition that
 * was named or of the host's stream never asks the policy, and after a
 * fallback that failed the policy still says fallback. The boot's mode is
 * the core's to keep, normal for those two and this after the policy's
 * report (F-260). Nothing calls it yet.
 */
enum boot_policy_mode boot_policy_mode(void);

/**
 * Command mode's 'boot activate' (boot.md 3.4)
 *
 * @return -PB_ERR_NOT_SUPPORTED without a policy, or where the policy has
 *         nothing to activate
 */
int boot_policy_activate(uuid_t part_uu);

/** Command mode's 'boot status' (boot.md 3.4). The null UUID without a policy */
void boot_policy_status(uuid_t part_uu);

#endif // INCLUDE_BOOT_POLICY_H
