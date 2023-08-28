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

#include <uuid.h>
#include <pb/bio.h>

enum ab_rollback_mode
{
    AB_ROLLBACK_MODE_NORMAL,
    AB_ROLLBACK_MODE_SPECULATIVE,
};

struct boot_ab_state_config
{
    const unsigned char *primary_state_part_uu;
    const unsigned char *backup_state_part_uu;
    const unsigned char *sys_a_uu;
    const unsigned char *sys_b_uu;
    enum ab_rollback_mode rollback_mode;
};

/**
 * Initialize the module
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
 * Compute which block device that should be used for booting.
 * This advances the boot counters if needed and may trigger rollback
 * on un-verified partitions.
 *
 * @return A valid block device on success,
 *        -PB_ERR_PARAM, bad block device UUID's
 *        -PB_ERR_IO, on read/write errors
 *        -PB_ERR_NOT_FOUND, when can't partitions can't be found
 */
bio_dev_t boot_ab_state_get(void);

/**
 * For partition 'part_uu' to be active and verified.
 *
 * @param[in] part_uu Partition to activate
 *
 * @return PB_OK on success,
 *        -PB_ERR_PARAM, bad block device UUID's
 *        -PB_ERR_IO, on read/write errors
 *        -PB_ERR_NOT_FOUND, when can't partitions can't be found
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
