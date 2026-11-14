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
int rot_revoke_key(key_id_t id);
int rot_get_dsa_key(key_id_t id, dsa_t *dsa_kind, const uint8_t **der_data, size_t *der_data_length);

#endif // INCLUDE_PB_ROT_H
