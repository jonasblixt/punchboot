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
#include <stdlib.h>
#include <string.h>

extern const struct bpak_keystore keystore_pb;
static const struct rot_config *cfg;
/* The key ids in slot order, empty: the keystore's order */
static const char *slots = CONFIG_KEYSTORE_SLOTS;

/* The next id in 'slots': 1, 0 at the end, or -PB_ERR_PARAM */
static int next_slot(const char **p, key_id_t *id)
{
    char *end;

    while (**p == ' ')
        (*p)++;

    if (**p == '\0')
        return 0;

    *id = strtoul(*p, &end, 16);

    if (end == *p || (*end != ' ' && *end != '\0'))
        return -PB_ERR_PARAM;

    *p = end;
    return 1;
}

static int key_by_id(key_id_t id, struct rot_key *key)
{
    const char *p = slots;
    key_id_t slot_id;
    int rc;

    if (*slots == '\0') {
        for (unsigned int i = 0; i < keystore_pb.no_of_keys; i++) {
            if (keystore_pb.keys[i]->id == id) {
                key->id = id;
                key->slot = i;
                return PB_OK;
            }
        }

        return -PB_ERR_KEY_NOT_FOUND;
    }

    for (unsigned int n = 0; (rc = next_slot(&p, &slot_id)) > 0; n++) {
        if (slot_id == id) {
            key->id = id;
            key->slot = n;
            return PB_OK;
        }
    }

    return (rc < 0) ? rc : -PB_ERR_KEY_NOT_FOUND;
}

int rot_init(const struct rot_config *cfg_)
{
    struct rot_key key;
    const char *p = slots;
    key_id_t id;
    int rc;

    if (cfg_ == NULL)
        return -PB_ERR_PARAM;

    while ((rc = next_slot(&p, &id)) > 0)
        ;

    if (rc < 0) {
        LOG_ERR("CONFIG_KEYSTORE_SLOTS: not a key id at '%s'", p);
        return rc;
    }

    for (unsigned int i = 0; i < keystore_pb.no_of_keys; i++) {
        if (key_by_id(keystore_pb.keys[i]->id, &key) != PB_OK) {
            LOG_ERR("CONFIG_KEYSTORE_SLOTS: no slot for key 0x%x", keystore_pb.keys[i]->id);
            return -PB_ERR_PARAM;
        }
    }

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

    LOG_INFO("Revoking key 0x%x, slot %u", key.id, key.slot);
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
