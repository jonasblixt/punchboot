#ifndef PB_INCLUDE_DER_HELPERS_H
#define PB_INCLUDE_DER_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pb/crypto.h>

/**
 * Extract r and s values from a DER encoded signature
 *
 * @param[in] sig DER bytes
 * @param[out] r r value output
 * @param[out] s s value output
 * @param[in] length Length of r and s buffers, they are expected to be
 *              the same length.
 * @param[in] suppress_leading_zero Suppresses any leading zero bytes if
 *              MSB is set in the next byte. See comment in the code.
 *
 * @return PB_OK, on success
 *        -PB_ERR_ASN1 Decoding errors
 *        -PB_ERR_BUFFER_TOO_SMALL, if output buffers are too small
 */
int der_ecsig_to_rs(const uint8_t *sig, uint8_t *r, uint8_t *s, size_t length,
                    bool suppress_leading_zero);

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

#endif  // PB_INCLUDE_DER_HELPERS_H
