/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This module provides a block io backed state for A/B partition selection
 * and rollback logic.
 *
 */
#ifndef BOOT_AB_STATE_H
#define BOOT_AB_STATE_H

#include <boot/policy.h>
#include <pb/bio.h>
#include <pb/utils_def.h>
#include <uuid.h>

/**
 * The board's configuration for A/B (board.md 2.5): the two slots and the
 * two state partitions. Speculative mode (boot.md 3.6) isn't the board's
 * to switch: CONFIG_BOOT_AB_SPECULATIVE is its only switch.
 */
struct boot_ab_state_config {
    const unsigned char *primary_state_part_uu;
    const unsigned char *backup_state_part_uu;
    const unsigned char *sys_a_uu;
    const unsigned char *sys_b_uu;
};

/**
 * Initialize the module
 *
 * After an init that failed every function of the module is safe to call:
 * without the state partitions nothing is read or written and the functions
 * that need the state return -PB_ERR_STATE. When the state partitions are
 * there and the state couldn't be read, boot_ab_state_set_boot_partition()
 * ('boot activate') writes a new one, and until it has, no candidate is
 * handed out and no board register is written (-PB_ERR_STATE).
 *
 * @param[in] cfg Module configuration
 *
 * @return PB_OK on success
 *        -PB_ERR_PARAM, bad block device UUID's
 *        -PB_ERR_IO, on read/write errors
 *        -PB_ERR_NOT_FOUND, when can't partitions can't be found
 */
int boot_ab_state_init(const struct boot_ab_state_config *cfg);

/**
 * Command mode's 'boot activate' (boot.md 3.5): the slot 'part_uu' is
 * enabled and marked verified, the other slot's verified bit and the error
 * bits are cleared, and the state is written. The null UUID disables both.
 *
 * @param[in] part_uu A slot's partition, or the null UUID
 *
 * @return PB_OK on success,
 *        -PB_ERR_STATE, before an init that found the state partitions
 *        -PB_ERR_PART_NOT_BOOTABLE, 'part_uu' is neither: nothing changes
 *         and nothing is written
 *         or the error of the state's write (-PB_ERR_IO)
 */
int boot_ab_state_set_boot_partition(uuid_t part_uu);

/**
 * Get current active boot partition
 *
 * @param[out] part_uu Active partition or empty UUID
 */
void boot_ab_state_get_boot_partition(uuid_t part_uu);

/**
 * Returns textual representation of 'part_uu' partition
 *
 * @param[in] part_uu Partition UUID to translate
 *
 * @return "A", when System A is active
 *         "B", when System B is active
 *         "?", All other states
 */
const char *boot_ab_part_uu_to_name(uuid_t part_uu);

/**
 * Read a board specific register
 *
 * @param[in] index Index of register to read
 * @param[out] value Pointer to output value
 *
 * @return 0 on success
 */
int boot_ab_state_read_board_reg(unsigned int index, uint32_t *value);

/**
 * Write a board specific register
 *
 * @param[in] index Index of register to write
 * @param[out] value Value to write
 *
 * @return 0 on success
 */
int boot_ab_state_write_board_reg(unsigned int index, uint32_t value);

extern const struct boot_policy_ops boot_policy_ab_ops;

/**
 * A board's boot source policy (board.boot_policy): A/B, with '_config', a
 * const struct boot_ab_state_config *.
 */
#define BOOT_POLICY_AB(_config)                                          \
    {                                                                    \
        .ops = &boot_policy_ab_ops,                                      \
        .config = PB_CONFIG(_config, const struct boot_ab_state_config), \
    }

#endif
