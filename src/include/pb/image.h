/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_IMAGE_H_
#define INCLUDE_PB_IMAGE_H_

#include <stdint.h>
#include <pb/crypto.h>
#include <bpak/bpak.h>
#include <pb-tools/wire.h>

typedef int (*pb_image_read_t) (void *buf, size_t size, void *private);
typedef int (*pb_image_result_t) (int rc, void *private);

struct bpak_header *pb_image_header(void);
int pb_image_check_header(void);
int pb_image_load(pb_image_read_t read_f,
                  pb_image_result_t result_f,
                  size_t load_chunk_size,
                  void *priv);

#endif  // INCLUDE_PB_IMAGE_H_
