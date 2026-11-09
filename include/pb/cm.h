/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_CM_H
#define INCLUDE_CM_H

#include <config.h>
#include <stddef.h>
#include <stdint.h>

/* With the gate a closed device opens only to a method: a build without one
 * answers NOT_SUPPORTED to AUTHENTICATE for good once locked
 * (command-mode.md 3.2, D-059) */
#if defined(CONFIG_CM_AUTH) && !defined(CONFIG_CM_AUTH_TOKEN) && !defined(CONFIG_CM_AUTH_PASSWORD)
#error "CONFIG_CM_AUTH needs a method: CONFIG_CM_AUTH_TOKEN or CONFIG_CM_AUTH_PASSWORD"
#endif

struct cm_transport_ops {
    int (*init)(void);
    int (*connect)(void);
    int (*disconnect)(void);
    int (*read)(void *bfr, size_t length);
    int (*write)(const void *bfr, size_t length);
    int (*complete)(void);
};

struct cm_config {
    const char *name;
    int (*status)(uint8_t *buf, size_t *length);
    int (*password_auth)(const char *password, size_t length);
    int (*command)(uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size);
    void (*process)(void);
    struct cm_transport_ops tops;
};

/**
 * Start command mode
 *
 * This function calls 'cm_board_init' (src/board.c), which sets up the
 * board's command mode hardware and calls 'cm_init'
 *
 * @return This function does not return normally
 */
int cm_run(void);

/**
 * Set up command mode for the board: board.cm_init(), then cm_init() with
 * the board's command mode config (src/board.c)
 *
 * @return PB_OK, on success
 */
int cm_board_init(void);

/**
 * Request a reboot from a board command.
 *
 * This function can be called by a board command to request a system reboot
 * after the result of the command has been send to the host.
 *
 * The reboot is only requested if the board command returns zero (PB_OK).
 */
void cm_board_cmd_request_reboot_on_success(void);

/**
 * Populate cm configuration struct
 *
 * @return PB_OK, on success
 */
int cm_init(const struct cm_config *cfg);

#endif // INCLUDE_CM_H
