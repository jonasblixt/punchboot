/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_PB_STATE_BLOB_H
#define INCLUDE_BOOT_PB_STATE_BLOB_H

/**
 * Note 1: board_regs
 * Board regs are accessed in reverse order to allow future expansion. If the
 * reserved array is further split, consider carving of another reserved space
 * to make sure it's possible to grow the board_regs.
 */

#include <stdint.h>

#define PB_STATE_MAGIC            0x026d4a65

#define PB_STATE_NO_OF_BOARD_REGS 4

struct pb_boot_state /* 512 bytes */
{
    uint32_t magic; /*!< PB boot state magic number */
    uint32_t enable; /*!< Boot partition enable bits */
    uint32_t verified; /*!< Boot partition verified bits */
    uint32_t remaining_boot_attempts; /*!< Rollback boot counter */
    uint32_t error; /*!< Rollback error bits */
    uint8_t rz[472]; /*!< Reserved, set to zero */
    uint32_t board_regs[PB_STATE_NO_OF_BOARD_REGS]; /*!< Board specific registers */
    uint32_t crc; /*!< State checksum */
} __attribute__((packed));

#define PB_STATE_A_ENABLED        (1 << 0)
#define PB_STATE_B_ENABLED        (1 << 1)
#define PB_STATE_A_VERIFIED       (1 << 0)
#define PB_STATE_B_VERIFIED       (1 << 1)

#define PB_STATE_ERROR_A_ROLLBACK (1 << 0)
#define PB_STATE_ERROR_B_ROLLBACK (1 << 1)

#endif
