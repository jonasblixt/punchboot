#include <keys.h>


u8 * pb_key_get(u8 key_index) {

    switch (key_index) {
        case PB_KEY_DEV:
            return (u8 *)&_binary____pki_dev_rsa_public_der_start;
        case PB_KEY_PROD:
            return (u8 *)&_binary____pki_prod_rsa_public_der_start;
        case PB_KEY_FIELD1:
            return (u8 *) &_binary____pki_field1_rsa_public_der_start;
        case PB_KEY_FIELD2:
            return (u8 *) &_binary____pki_field2_rsa_public_der_start;
        case PB_KEY_INV:
        default:
            return NULL;
    }
    
}



