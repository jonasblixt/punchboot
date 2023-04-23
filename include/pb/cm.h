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

#include <stdint.h>
#include <stddef.h>

struct cm_transport_ops {
    int (*init)(void);
    int (*connect)(void);
    int (*disconnect)(void);
    int (*read)(void *bfr, size_t length);
    int (*write)(const void *bfr, size_t length);
};

struct cm_config {
    const char *name;
    int (*status)(uint8_t *buf, size_t *length);
    int (*password_auth)(const char *password, size_t length);
    int (*command)(uint32_t cmd,
                   uint8_t *bfr, size_t bfr_length,
                   uint8_t *rsp, size_t *rsp_size);
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
 * Populate cm configuration struct
 *
 * @return PB_OK, on success
 */
int cm_init(const struct cm_config *cfg);

#endif  // INCLUDE_CM_H
