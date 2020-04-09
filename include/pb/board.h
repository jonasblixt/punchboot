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

struct pb_board;
struct pb_command_context;

typedef int (*pb_board_call_t) (struct pb_board *board);

struct pb_board
{
    const char *name;
    bool force_command_mode;
    pb_board_call_t pre_boot;
};

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
                          struct bpak_keystore *keystore,
                          struct pb_board *board);



#endif  // INCLUDE_PB_BOARD_H_
