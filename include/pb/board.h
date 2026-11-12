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
 * board defines 'board'. main() calls console_init() between
 * plat_early_init() and plat_init(), then src/board.c sets up the rest in
 * this order:
 *
 *  1. rot, slc       The keys and the life cycle, before anything that can
 *                    fail, so that command mode knows whether to ask for
 *                    authentication
 *  2. init()         Hardware: crypto, storage
 *  3. ptbl           The partition table
 *  4. boot_policy    The boot source policy
 *  5. late_init()    What needs the partitions
 *  6. boot_handover  The handover, and its delivery
 *
 * Command mode, cm_board_init(): cm_init(), then cm_transport.
 *
 * One member per concept (board.md 1). ptbl, boot_policy, boot_handover and
 * boot_delivery are objects: a module's functions and the board's
 * configuration for it, made by the module's macro. src/board.c calls
 * through them and knows no module.
 *
 * Hooks are optional (NULL), the rest is required unless it says otherwise.
 */

#ifndef INCLUDE_PB_BOARD_H
#define INCLUDE_PB_BOARD_H

#include <boot/handover.h>
#include <boot/policy.h>
#include <pb/ptbl.h>
#include <stddef.h>
#include <stdint.h>
#include <uuid.h>

struct bpak_header;
struct cm_transport_ops;
struct rot_config;
struct slc_config;

struct board {
    const char *name; /*!< The board id in command mode, at most 16 characters */

    void (*console_init)(void); /*!< The debug UART: clock, pads, console_init() */

    const struct rot_config *rot; /*!< Key revocation */
    const struct slc_config *slc; /*!< Security life cycle */

    int (*init)(void); /*!< Hardware: crypto, storage */

    /* The partition table: PTBL_GPT(), PTBL_STATIC() or PTBL_STATIC_MAPPED().
     * Absent: init() created the partitions, or there are none */
    struct ptbl ptbl;

    /* The boot source policy: BOOT_POLICY_FIXED(), BOOT_POLICY_CHAIN() or
     * BOOT_POLICY_AB(). Absent: no boot from storage */
    struct boot_policy boot_policy;

    int (*late_init)(void); /*!< After the partition table and the policy */

    /* The handover: BOOT_HANDOVER_IMAGE() or BOOT_HANDOVER_SIMPLE() */
    struct boot_handover boot_handover;
    /* Its delivery: BOOT_DELIVERY_TFA(). Absent: directly */
    struct boot_delivery boot_delivery;

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
 * A boot driver DTB patch callback (dtb_patch_cb): sets /chosen/bootargs to
 * board.bootargs, or board.bootargs_verbose with BOOT_FLAG_VERBOSE
 */
int board_patch_bootargs(void *fdt, int offset);

#endif // INCLUDE_PB_BOARD_H
