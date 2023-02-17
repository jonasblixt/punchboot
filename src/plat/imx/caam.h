/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_CAAM_H_
#define PLAT_IMX_CAAM_H_

#include <pb/pb.h>
#include <bpak/keystore.h>

/**
 * Global init of the caam module.
 *
 * @return PB_OK on sucess
 */
int imx_caam_init(void);

/**
 * Verify a signature
 *
 * @param[in] signature ANS.1 Encoded signature
 * @param[in] signature_len Length of ASN.1 input buffer
 * @param[in] hash Input hash to verify
 * @param[in] alg Hash algorithm
 * @param[in] key Public key
 *
 * @return PB_OK on success or a negative number
 */
int caam_pk_verify(uint8_t *signature, size_t signature_len,
                   uint8_t *hash, enum pb_hash_algs alg,
                   struct bpak_key *key);

/**
 * Initialize the hashing context
 *
 * @param[in] pb_alg Hash algorithm to use
 *
 * @return PB_OK on success
 */
int caam_hash_init(enum pb_hash_algs pb_alg);

/**
 * Perform a hash update
 *
 *  @param[in] buf Input byte buffer
 *  @param[in] length Length of input buffer
 *
 *  @return PB_OK on success otherwise a negative number
 */
int caam_hash_update(uint8_t *buf, size_t len);

/**
 * Finalize hashing operation and generate output digest
 *
 * @param[out] output Digest output buffer
 * @param[in] size Size in bytes of output buffer
 *
 * @return PB_OK on success or a negative number
 */
int caam_hash_output(uint8_t *output, size_t size);

#endif  // PLAT_IMX_CAAM_H_
