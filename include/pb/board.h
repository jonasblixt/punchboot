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
 * Command mode sets up nothing of the board's: its USB is the platform's
 * (plat_cm_init()) and its transport is linked (board.md 4.1, 4.2).
 *
 * One member per concept (board.md 1). ptbl, boot_policy, boot_handover and
 * boot_delivery are objects: a module's functions and the board's
 * configuration for it, made by the module's macro. src/board.c calls
 * through them and knows no module.
 *
 * Hooks are optional (NULL), the rest is required unless it says otherwise.
 *
 * What is required is checked before it is used (board.md 3). board_setup():
 * image_regions, rot and slc first, boot_policy and boot_handover when their
 * step comes, a GPT's layouts by its driver. A failed check is a failed step,
 * -PB_ERR_PARAM: no boot from storage, and the handover is still set up when
 * its own data passed. cm_board_init(): name, password_auth.
 * A failed check there means no command mode, and pb resets.
 */

#ifndef INCLUDE_PB_BOARD_H
#define INCLUDE_PB_BOARD_H

#include <boot/handover.h>
#include <boot/image_regions.h>
#include <boot/policy.h>
#include <pb/ptbl.h>
#include <stddef.h>
#include <stdint.h>
#include <uuid.h>

struct rot_config;
struct slc_config;

struct board {
    const char *name; /*!< The board id in command mode, 1 to 16 characters */

    void (*console_init)(void); /*!< The debug UART: clock, pads, console_init() */

    const struct rot_config *rot; /*!< Key revocation */
    /* Security life cycle. A board without one has an slc without
     * read_status, and its command mode is open */
    const struct slc_config *slc;

    int (*init)(void); /*!< Hardware: crypto, storage */

    /* The partition table: PTBL_GPT(), PTBL_STATIC() or PTBL_STATIC_MAPPED().
     * Absent: init() created the partitions, or there are none. A GPT has
     * at least one layout */
    struct ptbl ptbl;

    /* The boot source policy: BOOT_POLICY_FIXED(), BOOT_POLICY_CHAIN() or
     * BOOT_POLICY_AB() */
    struct boot_policy boot_policy;

    int (*late_init)(void); /*!< After the partition table and the policy */

    /* The handover: BOOT_HANDOVER_IMAGE() or BOOT_HANDOVER_FH() */
    struct boot_handover boot_handover;
    /* Its delivery: BOOT_DELIVERY_TFA(). Absent: directly */
    struct boot_delivery boot_delivery;

    /* The image data regions, the only memory an image's parts are loaded
     * into: BOOT_IMAGE_REGION() entries, then BOOT_IMAGE_REGION_END
     * (boot/image_regions.h). None holds anything of pb's. Absent: nothing is
     * loaded, and only a partition that is booted in place boots */
    const struct boot_image_region *image_regions;

    int (*early_boot)(void); /*!< Before the image is loaded */
    /* Before the jump. The verified header: boot_image_get_bpak_header()
     * (boot/image_helpers.h) */
    int (*late_boot)(uuid_t boot_part_uu);

    /* The kernel command line board_patch_bootargs() sets, and the one
     * with BOOT_FLAG_VERBOSE */
    const char *bootargs;
    const char *bootargs_verbose;

    int (*status)(uint8_t *buf, size_t *length); /*!< NULL: "Board status OK!" */
    /* The board's commands are named commands, not a member (pb/command.h,
     * D-076) */
    /* Required with CONFIG_CM_AUTH_PASSWORD */
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
