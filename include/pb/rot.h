/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_ROT_H
#define INCLUDE_PB_ROT_H

#include <pb/crypto.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** A key of pb_keys (pb/keys.h), as the platform revokes it */
struct rot_key {
    key_id_t id; /*!< Punchboot ID of the key */
    unsigned int slot; /*!< Its revoke slot, e.g. SRK n's revoke fuse bit */
};

/**
 * The platform's key revocation. The keys and their slots are the board's,
 * VERIFY_KEY<n> in its makefile.mk (board.md 7).
 */
struct rot_config {
    int (*revoke_key)(const struct rot_key *key); /*!< Optional */
    int (*read_key_status)(const struct rot_key *key); /*!< Without it no key can be used */
    /*! The slots, from 0, the SoC has a revoke bit for (platform.md 4.1): a
     *  key in a slot from here up can't be revoked. 0: every slot has one */
    unsigned int revoke_slots;
};

int rot_init(const struct rot_config *cfg);

/**
 * The number of keys in pb_keys (pb/keys.h)
 *
 * @param[out] count The number of keys. Not written on an error
 *
 * @return PB_OK, or -PB_ERR_NOT_SUPPORTED before rot_init()
 */
int rot_no_of_keys(size_t *count);
int rot_read_key_status_by_idx(unsigned int index);
int rot_read_key_status(key_id_t id);

/**
 * The id of the key at 'index', in pb_keys' order, the slots'
 *
 * @param[in] index Below rot_no_of_keys()'s count
 * @param[out] id The key's id. Not written on an error
 *
 * @return PB_OK, -PB_ERR_NOT_SUPPORTED before rot_init(), or
 *        -PB_ERR_PARAM for an index pb_keys doesn't have
 */
int rot_key_idx_to_id(unsigned int index, key_id_t *id);

/**
 * Whether the key at 'index' is in a slot the SoC has no revoke bit for
 * (platform.md 4.1, command-mode.md 5.2): one that can't be revoked.
 *
 * @param[in] index In pb_keys' order
 *
 * @return true for such a key; false for one that can be, for an index
 *         pb_keys doesn't have and before rot_init(): nothing to report
 */
bool rot_key_unrevocable_by_idx(unsigned int index);

/**
 * Revoke a key: the platform writes its slot's revoke bit
 *
 * @param[in] id The key's id
 *
 * @return PB_OK, also for a key that was revoked already;
 *         -PB_ERR_KEY_NOT_FOUND for an id pb_keys doesn't have;
 *         -PB_ERR_NOT_SUPPORTED without the platform's revoke_key, before
 *         rot_init(), and for a key in a slot the SoC can't revoke; or the
 *         platform's error
 */
int rot_revoke_key(key_id_t id);
int rot_get_dsa_key(key_id_t id, dsa_t *dsa_kind, const uint8_t **der_data, size_t *der_data_length);

#endif // INCLUDE_PB_ROT_H
