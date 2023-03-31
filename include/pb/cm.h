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
    int (*read)(uintptr_t bfr, size_t length);
    int (*write)(uintptr_t bfr, size_t length);
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

const struct cm_config * cm_board_init(void);
int cm_run(void);

#endif  // INCLUDE_CM_H
