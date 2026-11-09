/**
 * \file board.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A board is data: what is on the PCB and what the product decides. Each
 * board defines 'board', and src/board.c sets it up in this order:
 *
 *  1. rot, slc      The keys and the life cycle, before anything that can
 *                   fail, so that command mode knows whether to ask for
 *                   authentication
 *  2. init()        Hardware: crypto, storage
 *  3. gpt_*         The partition table, on the device gpt_dev_uu
 *  4. ab            The A/B boot state
 *  5. late_init()   What needs the partitions
 *  6. boot_*        The boot driver: boot_linux, boot_atf or boot_armv7m
 *
 * Command mode: cm_init(), then cm_transport.
 *
 * Hooks are optional (NULL), the rest is required unless it says otherwise.
 */

#ifndef INCLUDE_PB_BOARD_H
#define INCLUDE_PB_BOARD_H

#include <stddef.h>
#include <stdint.h>
#include <uuid.h>

struct bpak_header;
struct boot_ab_state_config;
struct boot_driver_armv7m_baremetal_config;
struct boot_driver_atf_config;
struct boot_driver_linux_config;
struct cm_transport_ops;
struct gpt_table_list;
struct rot_config;
struct slc_config;

struct board {
    const char *name; /*!< The board id in command mode, at most 16 characters */

    const struct rot_config *rot; /*!< Key revocation */
    const struct slc_config *slc; /*!< Security life cycle */

    int (*init)(void); /*!< Hardware: crypto, storage */

    const unsigned char *gpt_dev_uu; /*!< Device with a GPT, NULL: none */
    const struct gpt_table_list *gpt_tables; /*!< Its layouts, see board_layout_gpt */
    size_t gpt_tables_length;

    const struct boot_ab_state_config *ab; /*!< A/B state, see board_layout_ab. NULL: none */

    int (*late_init)(void); /*!< After the partition table and the A/B state */

    /* The boot driver: one of them */
    const struct boot_driver_linux_config *boot_linux;
    const struct boot_driver_atf_config *boot_atf;
    const struct boot_driver_armv7m_baremetal_config *boot_armv7m;
    int (*early_boot)(void); /*!< Before the image is loaded */
    int (*late_boot)(struct bpak_header *header, uuid_t boot_part_uu); /*!< Before the jump */

    /* The kernel command line board_patch_bootargs() sets, and the one
     * with BOOT_FLAG_VERBOSE */
    const char *bootargs;
    const char *bootargs_verbose;

    int (*cm_init)(void); /*!< Command mode's hardware, e.g. the USB controller */
    const struct cm_transport_ops *cm_transport; /*!< e.g. pb_dev_cls_transport */
    int (*status)(uint8_t *buf, size_t *length); /*!< NULL: "Board status OK!" */
    int (*command)(uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size);
    int (*password_auth)(const char *password, size_t length);
};

/** The board pb is built for, defined by the board */
extern const struct board board;

/**
 * Set up the board, steps 1-6 above
 *
 * @return PB_OK on success or a negative number
 */
int board_setup(void);

/**
 * Set up command mode for the board
 *
 * @return PB_OK on success or a negative number
 */
int board_cm_setup(void);

/**
 * A boot driver DTB patch callback (dtb_patch_cb): sets /chosen/bootargs to
 * board.bootargs, or board.bootargs_verbose with BOOT_FLAG_VERBOSE
 */
int board_patch_bootargs(void *fdt, int offset);

#endif // INCLUDE_PB_BOARD_H
