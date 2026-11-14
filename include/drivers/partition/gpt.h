/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_PARTITION_GPT_H
#define INCLUDE_DRIVERS_PARTITION_GPT_H

#include <pb/bio.h>
#include <pb/ptbl.h>
#include <pb/utils_def.h>
#include <stdint.h>

#define GPT_PART_NAME_MAX_SIZE 36

struct gpt_part_table {
    const unsigned char *uu;
    const char *description;
    size_t size;
};

struct gpt_table_list {
    const char *name;
    int variant;
    const struct gpt_part_table *table;
    size_t table_length;
};

int gpt_ptbl_init(bio_dev_t dev, const struct gpt_table_list *tables, size_t length);

extern const struct ptbl_ops ptbl_gpt_ops;

/**
 * A board's partition table (board.ptbl): a GPT on the device with the UUID
 * '_device'. '_tables' is the array of layouts command mode can install,
 * struct gpt_table_list[]: the array itself, its size is taken.
 */
#define PTBL_GPT(_device, _tables)                                      \
    {                                                                   \
        .ops = &ptbl_gpt_ops,                                           \
        .device = (_device),                                            \
        .table = PB_CONFIG(&(_tables)[0], const struct gpt_table_list), \
        .length = ARRAY_SIZE(_tables) + PB_MUST_BE_ARRAY(_tables),      \
    }

#endif // INCLUDE_DRIVERS_PARTITION_GPT_H
