/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/keys.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <stdint.h>

static const struct rot_config *cfg;

static const struct pb_key *find_key(key_id_t id)
{
    for (unsigned int i = 0; i < pb_no_of_keys; i++) {
        if (pb_keys[i].id == id)
            return &pb_keys[i];
    }

    return NULL;
}

static int key_by_id(key_id_t id, struct rot_key *key)
{
    const struct pb_key *k = find_key(id);

    if (k == NULL)
        return -PB_ERR_KEY_NOT_FOUND;

    key->id = id;
    key->slot = k->slot;
    return PB_OK;
}

int rot_init(const struct rot_config *cfg_)
{
    if (cfg_ == NULL)
        return -PB_ERR_PARAM;

    cfg = cfg_;
    return PB_OK;
}

int rot_read_key_status(key_id_t id)
{
    struct rot_key key;
    int rc;

    if (!cfg || !cfg->read_key_status)
        return -PB_ERR_NOT_SUPPORTED;

    rc = key_by_id(id, &key);

    if (rc != PB_OK)
        return rc;

    return cfg->read_key_status(&key);
}

int rot_no_of_keys(size_t *count)
{
    if (!cfg)
        return -PB_ERR_NOT_SUPPORTED;

    *count = pb_no_of_keys;
    return PB_OK;
}

int rot_read_key_status_by_idx(unsigned int index)
{
    if (!cfg || !cfg->read_key_status)
        return -PB_ERR_NOT_SUPPORTED;
    if (!(index < pb_no_of_keys))
        return -PB_ERR_PARAM;

    return rot_read_key_status(pb_keys[index].id);
}

int rot_key_idx_to_id(unsigned int index, key_id_t *id)
{
    if (!cfg)
        return -PB_ERR_NOT_SUPPORTED;
    if (!(index < pb_no_of_keys))
        return -PB_ERR_PARAM;

    *id = pb_keys[index].id;
    return PB_OK;
}

int rot_revoke_key(key_id_t id)
{
    struct rot_key key;
    int rc;

    if (!cfg || !cfg->revoke_key)
        return -PB_ERR_NOT_SUPPORTED;

    rc = key_by_id(id, &key);

    if (rc != PB_OK)
        return rc;

    LOG_INFO("Revoking key 0x%x, slot %u", key.id, key.slot);
    return cfg->revoke_key(&key);
}

int rot_get_dsa_key(key_id_t id, dsa_t *dsa_kind, const uint8_t **der_data, size_t *der_data_length)
{
    const struct pb_key *key;
    int rc;

    if (!cfg)
        return -PB_ERR_NOT_SUPPORTED;

    rc = rot_read_key_status(id);

    if (rc != PB_OK)
        return rc;

    key = find_key(id);

    if (key == NULL)
        return -PB_ERR_KEY_NOT_FOUND;

    *dsa_kind = key->kind;
    *der_data = key->data;
    *der_data_length = key->size;

    return PB_OK;
}
