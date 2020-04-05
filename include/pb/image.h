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

struct pb_image_load_context;

typedef int (*pb_image_read_t) (struct pb_image_load_context *ctx,
                                void *buf, size_t size);

typedef int (*pb_image_result_t) (struct pb_image_load_context *ctx, int rc);

struct pb_image_load_context
{
    struct bpak_header header __a4k;
    pb_image_read_t read;
    pb_image_result_t result;
    size_t chunk_size;
    uint8_t signature[512];
    size_t signature_sz;
    struct pb_result result_data;
    void *private;
};

int pb_image_load(struct pb_image_load_context *ctx,
                  struct pb_crypto *crypto,
                  struct bpak_keystore *keystore);

#endif  // INCLUDE_PB_IMAGE_H_
