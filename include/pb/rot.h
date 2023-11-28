/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_ROT_H
#define INCLUDE_PB_ROT_H

#include <pb/crypto.h>
#include <stdint.h>

struct rot_key {
    const char *name; /*<! Description of key */
    key_id_t id; /*<! Punchboot ID of key */
    uint32_t param1; /*<! Platform specific 1, for example fuse bit */
    uint32_t param2; /*<! Platform specific 2 */
};

struct rot_config {
    int (*revoke_key)(const struct rot_key *key);
    int (*read_key_status)(const struct rot_key *key);
    size_t key_map_length;
    const struct rot_key key_map[];
};

int rot_init(const struct rot_config *cfg);
size_t rot_no_of_keys(void);
int rot_read_key_status_by_idx(unsigned int index);
int rot_read_key_status(key_id_t id);
key_id_t rot_key_idx_to_id(unsigned int index);
int rot_revoke_key(key_id_t id);
int rot_get_dsa_key(key_id_t id, dsa_t *dsa_kind, const uint8_t **der_data, size_t *der_data_length);

#endif // INCLUDE_PB_ROT_H
