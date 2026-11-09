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

#include <stddef.h>
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

/** A fuse word the configuration sets, e.g. the boot device */
struct slc_fuse {
    uint32_t index; /*!< The platform's fuse index, e.g. IMX6UL_FUSE() */
    uint32_t value; /*!< The bits to set */
};

/**
 * The platform's life cycle, and the board's configuration fuses.
 * slc_set_configuration() calls set_configuration (e.g. the SRK hash), then
 * sets the bits of each of 'fuses' that aren't set yet.
 */
struct slc_config {
    slc_t (*read_status)(void);
    int (*set_configuration)(void); /*!< Optional with 'fuses' */
    int (*set_configuration_locked)(void);
    int (*set_eol)(void);
    const struct slc_fuse *fuses;
    size_t fuses_length;
    int (*fuse_read)(uint32_t index, uint32_t *value); /*!< Required with 'fuses' */
    int (*fuse_write)(uint32_t index, uint32_t value);
};

int slc_init(const struct slc_config *cfg);
slc_t slc_read_status(void);
int slc_set_configuration(void);
int slc_set_configuration_locked(void);
int slc_set_eol(void);

#endif // INCLUDE_PB_SLC_H
