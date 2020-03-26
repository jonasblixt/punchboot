/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_IMAGE_H_
#define INCLUDE_PB_IMAGE_H_

#include <stdint.h>
#include <bpak/bpak.h>

uint32_t pb_image_check_header(struct bpak_header *h);

uint32_t pb_image_load_from_fs(uint64_t part_lba_start,
                                uint64_t part_lba_end,
                                struct bpak_header *h,
                                const char *hash);
#endif  // INCLUDE_PB_IMAGE_H_
