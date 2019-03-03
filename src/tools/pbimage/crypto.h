#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#include <stdint.h>
#include <stdbool.h>

uint32_t crypto_initialize(void);

uint32_t crypto_sign(uint8_t *hash,
                     uint32_t hash_kind,
                     const char *key_source,
                     uint32_t sign_kind,
                     uint8_t *out);


#endif
