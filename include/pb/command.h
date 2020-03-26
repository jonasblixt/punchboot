/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_RECOVERY_H_
#define INCLUDE_PB_RECOVERY_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb/storage.h>

struct pb_command_ctx
{
    struct pb_storage *storage;
    bool authenticated;
};

int command_initialize(struct pb_storage *storage);

#endif  // INCLUDE_PB_RECOVERY_H_
