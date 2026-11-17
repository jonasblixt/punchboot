/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
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

/** A fuse word the configuration sets: a word of the key hash (the SRK hash,
 *  <P>_FUSE_SRK(n)), the boot device (D-094) */
struct slc_fuse {
    uint32_t index; /*!< The platform's fuse index, e.g. IMX6UL_FUSE() */
    uint32_t value; /*!< The bits to set, not 0 */
};

/**
 * The platform's life cycle operations, plat_slc (pb/plat.h). They only read
 * and write fuses: the order of the states is checked in slc.c.
 */
struct slc_ops {
    slc_t (*read_status)(void); /*!< Without it there is no life cycle */
    int (*set_configuration)(void); /*!< Optional: what else configures the SoC */
    int (*set_configuration_locked)(void);
    int (*set_eol)(void); /*!< Called with CONFIG_SLC_EOL only (system.md 4) */
    int (*fuse_read)(uint32_t index, uint32_t *value); /*!< Required with the board's fuses */
    int (*fuse_write)(uint32_t index, uint32_t value);
};

/**
 * Set up the life cycle: the platform's operations and the board's
 * configuration fuses (board.slc_fuses), the key hash among them.
 * slc_set_configuration() calls set_configuration when the platform has it,
 * reads every one of 'fuses', and refuses with nothing written when a word
 * has a bit its value lacks; then it sets the bits that aren't set yet.
 *
 * @param[in] ops The platform's operations, plat_slc
 * @param[in] fuses The board's configuration fuses, NULL for none
 * @param[in] fuses_length Their number
 *
 * @return PB_OK, or -PB_ERR_PARAM for fuses without a list of them, an
 *        entry whose value is 0, or without the platform's fuse_read and
 *        fuse_write: nothing is set up
 */
int slc_init(const struct slc_ops *ops, const struct slc_fuse *fuses, size_t fuses_length);
slc_t slc_read_status(void);
/**
 * Writes the configuration (command-mode.md 5.2): see slc_init()
 *
 * @return PB_OK, also when every bit is set already,
 *        -PB_ERR_STATE when locked or in EOL, or a word has a bit its value
 *         lacks (nothing is written),
 *        -PB_ERR_NOT_SUPPORTED without anything to write or without fuse
 *         writes (CONFIG_SLC_NO_FUSE_WRITES),
 *         or the platform's error
 */
int slc_set_configuration(void);
int slc_set_configuration_locked(void);
/**
 * Moves the device to end of life, from any state that is known
 *
 * @return PB_OK, also when the device is in EOL already,
 *        -PB_ERR_NOT_SUPPORTED without CONFIG_SLC_EOL, the product's option
 *         (system.md 4), and where the port has no set_eol,
 *         the error of a life cycle that can't be read, or the port's
 */
int slc_set_eol(void);

#endif // INCLUDE_PB_SLC_H
