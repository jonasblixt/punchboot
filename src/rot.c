/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <bpak/bpak.h>
#include <bpak/id.h>
#include <bpak/keystore.h>
#include <pb/pb.h>
#include <pb/rot.h>

extern const struct bpak_keystore keystore_pb;
static const struct rot_config *cfg;

int rot_init(const struct rot_config *cfg_)
{
    if (cfg_ == NULL)
        return -PB_ERR_PARAM;

    cfg = cfg_;
    return PB_OK;
}

/* The keys are the keystore's, in its order */
static int key_by_id(key_id_t id, struct rot_key *key)
{
    for (unsigned int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id == id) {
            key->id = id;
            key->index = i;
            return PB_OK;
        }
    }

    return -PB_ERR_KEY_NOT_FOUND;
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

size_t rot_no_of_keys(void)
{
    if (!cfg)
        return -PB_ERR_NOT_SUPPORTED;
    return keystore_pb.no_of_keys;
}

int rot_read_key_status_by_idx(unsigned int index)
{
    if (!cfg || !cfg->read_key_status)
        return -PB_ERR_NOT_SUPPORTED;
    if (!(index < keystore_pb.no_of_keys))
        return -PB_ERR_PARAM;

    return rot_read_key_status(keystore_pb.keys[index]->id);
}

key_id_t rot_key_idx_to_id(unsigned int index)
{
    if (!cfg)
        return -PB_ERR_NOT_SUPPORTED;
    if (!(index < keystore_pb.no_of_keys))
        return -PB_ERR_PARAM;

    return keystore_pb.keys[index]->id;
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

    LOG_INFO("Revoking key %u (0x%x)", key.index, key.id);
    return cfg->revoke_key(&key);
}

int rot_get_dsa_key(key_id_t id, dsa_t *dsa_kind, const uint8_t **der_data, size_t *der_data_length)
{
    const struct bpak_key *key = NULL;
    int rc;

    if (!cfg)
        return -PB_ERR_NOT_SUPPORTED;

    rc = rot_read_key_status(id);

    if (rc != PB_OK)
        return rc;

    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id == id) {
            key = keystore_pb.keys[i];
            break;
        }
    }

    if (key == NULL)
        return -PB_ERR_KEY_NOT_FOUND;

    switch (key->kind) {
    case BPAK_KEY_PUB_PRIME256v1:
        *dsa_kind = DSA_EC_SECP256r1;
        break;
    case BPAK_KEY_PUB_SECP384r1:
        *dsa_kind = DSA_EC_SECP384r1;
        break;
    case BPAK_KEY_PUB_SECP521r1:
        *dsa_kind = DSA_EC_SECP521r1;
        break;
    default:
        return -PB_ERR_NOT_SUPPORTED;
    }

    *der_data = key->data;
    *der_data_length = key->size;

    return PB_OK;
}
