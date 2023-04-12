/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/rot.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <bpak/keystore.h>

extern const struct bpak_keystore keystore_pb;
static const struct rot_config *cfg;

int rot_init(const struct rot_config *cfg_)
{
    /* Check that the number of keys and key ID's match
     * between bundeled bpak keystore and what the board config claims */
    if (keystore_pb.no_of_keys != cfg_->key_map_length)
        return -PB_ERR_MEM;

    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (keystore_pb.keys[i]->id != cfg_->key_map[i].id) {
            return -PB_ERR_MEM;
        }
    }

    cfg = cfg_;

    return PB_OK;
}

int rot_read_key_status(key_id_t id)
{
    if (!cfg || !cfg->read_key_status)
        return -PB_ERR_NOT_SUPPORTED;

    for (unsigned int i = 0; i < cfg->key_map_length; i++) {
        if (cfg->key_map[i].id == id) {
            return cfg->read_key_status(&cfg->key_map[i]);
        }
    }

    return -PB_ERR_KEY_NOT_FOUND;
}

size_t rot_no_of_keys(void)
{
    if (!cfg)
        return -PB_ERR_NOT_SUPPORTED;
    return cfg->key_map_length;
}

int rot_read_key_status_by_idx(unsigned int index)
{
    if (!cfg || !cfg->read_key_status)
        return -PB_ERR_NOT_SUPPORTED;
    if (!(index < cfg->key_map_length))
        return -PB_ERR_PARAM;

    return cfg->read_key_status(&cfg->key_map[index]);
}

key_id_t rot_key_idx_to_id(unsigned int index)
{
    if (!cfg)
        return -PB_ERR_NOT_SUPPORTED;
    if (!(index < cfg->key_map_length))
        return -PB_ERR_PARAM;

    return cfg->key_map[index].id;
}

int rot_revoke_key(key_id_t id)
{
    if (!cfg || !cfg->revoke_key)
        return -PB_ERR_NOT_SUPPORTED;

    if (!imx8x_is_srk_fused())
        return -PB_ERR_MEM;

    for (unsigned int i = 0; i < cfg->key_map_length; i++) {
        if (cfg->key_map[i].id == id) {
            LOG_INFO("Revoking key: %s (0x%x)", cfg->key_map[i].name,
                                                cfg->key_map[i].id);
            return cfg->revoke_key(&cfg->key_map[i]);
        }
    }

    return -PB_ERR_KEY_NOT_FOUND;
}

int rot_get_dsa_key(key_id_t id,
                    dsa_t *dsa_kind,
                    const uint8_t **der_data, 
                    size_t *der_data_length)
{
    const struct bpak_key *key = NULL;
    int rc;

    if (!cfg || !cfg->key_map || !cfg->key_map_length)
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
