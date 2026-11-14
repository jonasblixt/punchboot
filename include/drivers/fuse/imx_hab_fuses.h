/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * HABv4's state in the OCOTP: the life cycle (closed bit, SRK hash) and the
 * SRK revocation. The same on every i.MX with HABv4, only the fuse
 * locations differ: the platform gives them to imx_hab_fuses_init(), and
 * the board puts the callbacks it wants in its rot_config and slc_config.
 *
 * The keystore's key n is SRK n: they share the revoke bit (pb/rot.h).
 */

#ifndef INCLUDE_DRIVERS_FUSE_IMX_HAB_FUSES_H
#define INCLUDE_DRIVERS_FUSE_IMX_HAB_FUSES_H

#include <pb/rot.h>
#include <pb/slc.h>
#include <stddef.h>
#include <stdint.h>

#define IMX_HAB_SRK_HASH_WORDS (8)

struct imx_hab_fuses {
    unsigned int sec_config_bank; /*!< The word with the closed bit, SEC_CONFIG */
    unsigned int sec_config_word;
    uint32_t closed; /*!< Its closed bit */
    unsigned int srk_bank; /*!< The first word of SRK_HASH */
    unsigned int srk_word;
    unsigned int revoke_bank; /*!< SRK_REVOKE */
    unsigned int revoke_word;
    unsigned int revocable; /*!< SRK0 .. revocable - 1 can be revoked */
    int (*revoke_allowed)(void); /*!< Optional, e.g. HAB reports no errors */
};

void imx_hab_fuses_init(const struct imx_hab_fuses *cfg);

/** NOT_CONFIGURED, CONFIGURATION once any SRK hash word is fused, or LOCKED */
slc_t imx_hab_slc_read_status(void);

/** Fuses the SRK hash of the build, srk_fuse (pb/srk_fuse.h) */
int imx_hab_slc_set_configuration(void);

/** Fuses the closed bit. slc.c only calls it in CONFIGURATION. */
int imx_hab_slc_set_configuration_locked(void);

int imx_hab_read_key_status(const struct rot_key *key);
int imx_hab_revoke_key(const struct rot_key *key);

/**
 * Fuse the SRK hash. A word that has bits set already must have none the
 * hash's word doesn't: a word a failed write left half programmed is
 * completed, one with another bit is -PB_ERR_MEM and nothing is written.
 *
 * @param srk The hash, e.g. srk_fuse (pb/srk_fuse.h)
 * @param words Its length, IMX_HAB_SRK_HASH_WORDS
 */
int imx_hab_fuse_srk(const uint32_t *srk, size_t words);

#endif // INCLUDE_DRIVERS_FUSE_IMX_HAB_FUSES_H
