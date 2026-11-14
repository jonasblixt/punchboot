/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_IMAGE_H
#define INCLUDE_BOOT_IMAGE_H

#include <boot/boot.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <pb/errors.h>
#include <stdbool.h>
#include <string.h>

/**
 * The load address of the part 'part_id': its pb-load-addr meta, a u64 as
 * the bpak CLI's integer encoder writes it (compatibility.md).
 *
 * The address is made of the meta's own bytes, whatever the meta's size is
 * (F-065): every reader of an image's load addresses asks here, so that what
 * is placed is what is loaded and jumped to.
 *
 * - 8 bytes or more: the first 8, as every release read them.
 * - Fewer: those, zero extended. It is what a release read from a header
 *   that has zeros after the meta, which is how bpak lays one out (a value
 *   starts at a multiple of 8). The bytes after it are no longer read.
 * - None: the part has no load address.
 *
 * @param[in] hdr A header that bpak_valid_header() accepts
 * @param[in] part_id The part
 * @param[out] addr In 64 bits, as the image has it
 *
 * @return PB_OK, or -PB_ERR_BAD_META for a part without a load address
 */
static inline int boot_image_load_addr(struct bpak_header *hdr, bpak_id_t part_id, uint64_t *addr)
{
    struct bpak_meta_header *mh;
    uint64_t value = 0;

    if (bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, part_id, &mh) != BPAK_OK || mh->size == 0)
        return -PB_ERR_BAD_META;

    memcpy(&value,
           bpak_get_meta_ptr(hdr, mh, uint8_t),
           (mh->size < sizeof(value)) ? mh->size : sizeof(value));
    *addr = value;
    return PB_OK;
}

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

/**
 * Placement of an image that is loaded (boot.md 4.3): every part has a load
 * address, lies inside one of the board's image data regions
 * (boot/image_regions.h) and overlaps no other part. Before anything is
 * loaded. An image with a part outside the payload's hash (bpak's dont-hash
 * flag) is refused here (boot.md 4.2).
 *
 * @param[in] hdr The image's header
 *
 * @return PB_OK when every part is placed
 *        -PB_ERR_BAD_HEADER, on bad header magic
 *        -PB_ERR_NOT_SUPPORTED, a part outside the payload's hash
 *        -PB_ERR_BAD_META, a part without a load address
 *        -PB_ERR_MEM, a part outside the regions or over another part
 */
int boot_image_verify_parts(struct bpak_header *hdr);

/**
 * Placement of an image that is booted in place: every part has a load
 * address and lies inside the mapped range of its partition, without the
 * header. Nothing is loaded, so pb's own memory isn't checked. A part
 * outside the payload's hash is refused as for an image that is loaded.
 *
 * @param[in] hdr The image's header
 * @param[in] base Address of the range's first byte
 * @param[in] size Size of the range in bytes
 *
 * @return PB_OK when every part is inside the range
 *        -PB_ERR_BAD_HEADER, on bad header magic
 *        -PB_ERR_NOT_SUPPORTED, a part outside the payload's hash
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

/**
 * Whether the candidate is verified in place (boot.md 4.1): its parts are
 * where its partition is mapped, a flash, and weren't copied to RAM. For a
 * handover's prepare, which must not grow or patch a part then.
 *
 * @return true for a partition that is booted in place, false for one that
 *  is loaded and for the host's stream
 */
bool boot_image_in_place(void);

#endif
