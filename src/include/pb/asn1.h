#ifndef INCLUDE_PB_ASN1_H_
#define INCLUDE_PB_ASN1_H_

#include <stdint.h>
#include <stddef.h>
#include <pb/crypto.h>
#include <bpak/keystore.h>

int pb_asn1_eckey_data(struct bpak_key *k, uint8_t **data, size_t *key_sz,
                        bool include_compression_point);

int pb_asn1_ecsig_to_rs(uint8_t *sig, uint8_t sig_kind,
                            uint8_t **r, uint8_t **s);

int pb_asn1_rsa_data(struct bpak_key *k, uint8_t **mod, uint8_t **exp);

int pb_asn1_size(unsigned char **p, size_t *len);

#endif  // INCLUDE_PB_ASN1_H_
