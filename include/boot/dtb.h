/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_DTB_H
#define INCLUDE_BOOT_DTB_H

#include <bpak/bpak.h>
#include <stddef.h>

/**
 * Make room in a loaded device tree for what the boot drivers add to it
 *
 * The device tree must fit its part. It is grown in place by headroom bytes
 * (fdt_open_into), into the RAM after the part: the part's size plus headroom
 * from its load address must not overlap another part of the image or pb's
 * own memory. That is checked before the device tree is read.
 *
 * @param[in] hdr The verified image header
 * @param[in] dtb_id bpak id of the device tree part
 * @param[in] headroom Bytes to grow the device tree by
 * @param[out] fdt The device tree, at its load address
 *
 * @return PB_OK on success,
 *        -PB_ERR_BAD_META if the part or its load address is missing,
 *        -PB_ERR_MEM if the grown device tree would overlap a part or pb,
 *        -PB_ERR if it isn't a device tree or is larger than its part
 */
int boot_dtb_grow(struct bpak_header *hdr, bpak_id_t dtb_id, size_t headroom, void **fdt);

#endif
