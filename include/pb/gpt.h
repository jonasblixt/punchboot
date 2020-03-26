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

int pb_gpt_map_init(struct pb_storage_driver *drv);

#endif  // INCLUDE_PB_GPT_H_
