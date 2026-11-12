/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_IMAGE_H
#define INCLUDE_BOOT_IMAGE_H

#include <boot/boot.h>
#include <bpak/bpak.h>

/**
 * Authenticate a BPAK header
 *
 * This function will verify the signature of the supplied bpak header
 *
 * @param[in] hdr Pointer to header that should be verified
 *
 * @return PB_OK, on verification success
 *        -PB_ERR_BAD_HEADER, on bad header magic
 *        -PB_ERR_BAD_KEYSTORE, if keystore is invalid or unavailable
 *        -PB_ERR_KEY_NOT_FOUND, if the key id in the header is not found
 *        -PB_ERR_KEY_REVOKED, if key has been revoked
 *        -PB_ERR_UNKNOWN_HASH, Unknown hash
 *        -PB_ERR_MEM, If signature data is too large
 *        -PB_ERR_SIGNATURE, on signature verification failure
 */
int boot_image_auth_header(struct bpak_header *hdr);

int boot_image_verify_parts(struct bpak_header *hdr);

/**
 * Placement of an image that is booted in place: every part has a load
 * address and lies inside the mapped range of its partition, without the
 * header. Nothing is loaded, so pb's own memory isn't checked.
 *
 * @param[in] hdr The image's header
 * @param[in] base Address of the range's first byte
 * @param[in] size Size of the range in bytes
 *
 * @return PB_OK when every part is inside the range
 *        -PB_ERR_BAD_HEADER, on bad header magic
 *        -PB_ERR_BAD_META, a part without a load address
 *        -PB_ERR_MEM, a part outside the range
 */
int boot_image_verify_parts_in_place(struct bpak_header *hdr, uintptr_t base, size_t size);

int boot_image_load_and_hash(struct bpak_header *hdr,
                             size_t load_chunk_size,
                             boot_read_cb_t read_f,
                             boot_result_cb_t result_f,
                             uint8_t *payload_digest,
                             size_t payload_digest_size);

int boot_image_copy_and_hash(struct bpak_header *source_hdr,
                             uintptr_t source_address,
                             size_t source_size,
                             uintptr_t destination_address,
                             size_t destination_size,
                             uint8_t *payload_digest,
                             size_t payload_digest_size);

int boot_image_verify_payload(struct bpak_header *hdr, uint8_t *payload_digest);

/**
 * The header of the image this boot verified (board.md 5.3), for a board that
 * needs it in its DT hook or in late_boot(). Read only: nothing that was
 * verified is changed.
 *
 * Valid from prepare to the jump. A pointer from an earlier call isn't kept:
 * the next candidate's header is read into the same memory.
 *
 * @return The header, or NULL: before the payload's hash of the candidate is
 *  checked, after a candidate's attempt failed, and once the boot has
 *  stopped or failed. Nothing unverified is handed out.
 */
const struct bpak_header *boot_image_get_bpak_header(void);

#endif
