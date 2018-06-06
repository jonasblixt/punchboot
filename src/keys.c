/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <keys.h>


uint8_t * pb_key_get(uint8_t key_index) {

    switch (key_index) {
        case PB_KEY_DEV:
            return (uint8_t *)&_binary____pki_dev_rsa_public_der_start;
        case PB_KEY_PROD:
            return (uint8_t *)&_binary____pki_prod_rsa_public_der_start;
        case PB_KEY_FIELD1:
            return (uint8_t *) &_binary____pki_field1_rsa_public_der_start;
        case PB_KEY_FIELD2:
            return (uint8_t *) &_binary____pki_field2_rsa_public_der_start;
        case PB_KEY_INV:
        default:
            return NULL;
    }
    
}



