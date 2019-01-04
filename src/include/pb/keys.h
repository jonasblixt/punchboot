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

#define PB_KEY_DEV    0
#define PB_KEY_PROD   1
#define PB_KEY_FIELD1 2
#define PB_KEY_FIELD2 3

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


struct asn1_key {
    uint8_t rz1[0x21];
    uint8_t mod[512];
    uint8_t rz2[2];
    uint8_t exp[3];
} __attribute__ ((packed));

struct asn1_key * pb_key_get(uint8_t key_index);

#endif
