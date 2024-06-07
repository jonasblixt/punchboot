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

#include <stddef.h>
#include <stdint.h>

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
    const struct cm_transport_ops tops;
};

/**
 * Start command mode
 *
 * This function will call 'cm_board_init' which should be provided by platform
 * or board level code.
 *
 * The board init should initialize any required hardware and memory that
 * cm needs.
 *
 * The board init function must provide a configuration struct by calling
 * 'cm_init'
 *
 * @return This function does not return normally
 */
int cm_run(void);

/**
 * Perform platform/board specific hardware inititalization
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
