/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_BOARD_H_
#define INCLUDE_PB_BOARD_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb/pb.h>
#include <pb/fuse.h>
#include <pb/gpt.h>
#include <pb/storage.h>
#include <pb/transport.h>
#include <pb/console.h>
#include <pb/crypto.h>
#include <pb/command.h>
#include <pb/boot.h>
#include <board/config.h>
#include <plat/defs.h>

/**
 * Function: board_early_init
 *
 * Called as early as possible by platform init code
 *
 * Return value: PB_OK if there is no error
 */
int board_early_init(struct pb_platform_setup *plat,
                          struct pb_storage *storage,
                          struct pb_transport *transport,
                          struct pb_console *console,
                          struct pb_crypto *crypto,
                          struct pb_command_context *command_ctx,
                          struct pb_boot_context *boot,
                          struct bpak_keystore *keystore);

/**
 * Function: board_late_init
 *
 * Called as a last step in the platform init code
 *
 * Return value: PB_OK if there is no error
 */
uint32_t board_late_init(struct pb_platform_setup *plat);

/**
 * Function: board_prepare_recovery
 *
 * Called before recovery mode is to be initialized and before
 *  usb controller is initialized.
 *
 * Return value: PB_OK if there is no error
 */
uint32_t board_prepare_recovery(struct pb_platform_setup *plat);

/**
 * Function: board_force_recovery
 *
 * Called during initialization and forces recovery mode depending on return
 *   value.
 *
 * Return value: True - Force recovery
 *               False - Normal boot
 */
bool  board_force_recovery(struct pb_platform_setup *plat);

uint32_t board_setup_device(void);
uint32_t board_setup_lock(void);
uint32_t board_recovery_command(uint32_t arg0, uint32_t arg1, uint32_t arg2,
                                uint32_t arg3);

const char * board_get_id(void);

#endif  // INCLUDE_PB_BOARD_H_
