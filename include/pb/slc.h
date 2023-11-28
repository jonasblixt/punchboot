/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_SLC_H
#define INCLUDE_PB_SLC_H

#include <stdint.h>

typedef int slc_t;

enum slc {
    SLC_INVALID,
    SLC_NOT_CONFIGURED,
    SLC_CONFIGURATION,
    SLC_CONFIGURATION_LOCKED,
    SLC_EOL,
    SLC_END,
};

struct slc_config {
    slc_t (*read_status)(void);
    int (*set_configuration)(void);
    int (*set_configuration_locked)(void);
    int (*set_eol)(void);
};

int slc_init(const struct slc_config *cfg);
slc_t slc_read_status(void);
int slc_set_configuration(void);
int slc_set_configuration_locked(void);
int slc_set_eol(void);

#endif // INCLUDE_PB_SLC_H
