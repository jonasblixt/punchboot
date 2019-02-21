/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <keys.h>
#include <fuse.h>
#include <plat.h>
#include <board.h>

struct asn1_key * pb_key_get(uint8_t key_index) 
{

    switch (key_index) 
    {
        case PB_KEY_DEV:
            return (struct asn1_key *)&_binary____pki_dev_rsa_public_der_start;
        case PB_KEY_PROD:
            return (struct asn1_key *)&_binary____pki_prod_rsa_public_der_start;
        case PB_KEY_FIELD1:
            return (struct asn1_key *) &_binary____pki_field1_rsa_public_der_start;
        case PB_KEY_FIELD2:
            return (struct asn1_key *) &_binary____pki_field2_rsa_public_der_start;
        default:
            return NULL;
    }
}

extern struct fusebox pb_fusebox;

uint32_t pb_update_key_revoke_mask(uint32_t mask)
{
    if (mask == 0)
        return PB_OK;

    return board_update_revoke_mask(mask);
}

uint32_t pb_is_key_revoked(uint8_t key_index, bool *result)
{
    uint32_t err;
    uint32_t revoke_mask = 0xFFFFFFFF;

    *result = true;

    if (key_index > 31)
        return PB_ERR;

    err = board_read_revoke_mask(&revoke_mask);

    if (err != PB_OK)
        return err;

    uint32_t key_bit = (1 << key_index);

    if ((revoke_mask & key_bit) == key_bit)
        *result = true;
    else
        *result = false;

    return PB_OK;
}

