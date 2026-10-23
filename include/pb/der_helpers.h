#ifndef PB_INCLUDE_DER_HELPERS_H
#define PB_INCLUDE_DER_HELPERS_H

#include <pb/crypto.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Extract r and s values from a DER encoded signature
 *
 * Leading zero bytes are dropped and shorter values are zero padded, so that
 * r and s are big-endian values of exactly 'length' bytes.
 *
 * @param[in] sig DER bytes
 * @param[in] sig_length Number of bytes in sig, nothing past it is read
 * @param[out] r r value output, 'length' bytes
 * @param[out] s s value output, 'length' bytes
 * @param[in] length Length of r and s, the curve size in bytes
 *
 * @return PB_OK, on success
 *        -PB_ERR_ASN1 Decoding errors
 *        -PB_ERR_BUF_TOO_SMALL, if r or s doesn't fit in 'length' bytes
 */
int der_ecsig_to_rs(const uint8_t *sig, size_t sig_length, uint8_t *r, uint8_t *s, size_t length);

/**
 * Extract public key data from a DER encoded key
 *
 * @param[in] pub_key_der DER bytes
 * @param[out] output Raw key output
 * @param[in] output_length Length of output buffer
 * @param[out] key_kind Decoded key type
 *
 * @return PB_OK, on success
 *        -PB_ERR_ASN1, on decoding errors
 *        -PB_ERR_BUFFER_TOO_SMALL on output buffer too small
 *        -PB_ERR_NOT_SUPPORTED, on un-supported key types
 */
int der_ec_public_key_data(const uint8_t *pub_key_der,
                           uint8_t *output,
                           size_t output_length,
                           dsa_t *key_kind);

int asn1_size(const unsigned char **p, size_t *len);

#endif // PB_INCLUDE_DER_HELPERS_H
