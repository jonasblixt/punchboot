/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __KEYS_H__
#define __KEYS_H__

#include <pb.h>

#define PB_KEY_INV    0
#define PB_KEY_DEV    1
#define PB_KEY_PROD   2
#define PB_KEY_FIELD1 3
#define PB_KEY_FIELD2 4

extern uint8_t* _binary____pki_dev_rsa_public_der_start;
extern uint8_t* _binary____pki_dev_rsa_public_der_end;
extern uint8_t* _binary____pki_dev_rsa_public_der_size;

extern uint8_t* _binary____pki_prod_rsa_public_der_start;
extern uint8_t* _binary____pki_prod_rsa_public_der_end;
extern uint8_t* _binary____pki_prod_rsa_public_der_size;

extern uint8_t* _binary____pki_field1_rsa_public_der_start;
extern uint8_t* _binary____pki_field1_rsa_public_der_end;
extern uint8_t* _binary____pki_field1_rsa_public_der_size;

extern uint8_t* _binary____pki_field2_rsa_public_der_start;
extern uint8_t* _binary____pki_field2_rsa_public_der_end;
extern uint8_t* _binary____pki_field2_rsa_public_der_size;


uint8_t * pb_key_get(uint8_t key_index);

#endif
