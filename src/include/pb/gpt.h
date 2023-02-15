/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_GPT_H_
#define INCLUDE_PB_GPT_H_

#include <pb/pb.h>
#include <pb/storage.h>

#define GPT_PART_NAME_MAX_SIZE 36


int gpt_install_map(struct pb_storage_driver *drv,
                            struct pb_storage_map *map);

int gpt_resize_map(struct pb_storage_driver *drv, struct pb_storage_map *map,
                    size_t blocks);

int gpt_init(struct pb_storage_driver *drv);

#endif  // INCLUDE_PB_GPT_H_
