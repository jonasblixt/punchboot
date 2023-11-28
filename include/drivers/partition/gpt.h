/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_PARTITION_GPT_H
#define INCLUDE_DRIVERS_PARTITION_GPT_H

#include <pb/bio.h>
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

#endif // INCLUDE_DRIVERS_PARTITION_GPT_H
