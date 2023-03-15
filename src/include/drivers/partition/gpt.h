/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_GPT_PTBL_H
#define INCLUDE_PB_GPT_PTBL_H

#include <stdint.h>
#include <pb/bio.h>

#define GPT_PART_NAME_MAX_SIZE 36

struct gpt_part_table {
    const unsigned char * uu;
    const char * description;
    size_t size;
    bool valid;
};

int gpt_ptbl_init(bio_dev_t dev, const struct gpt_part_table *default_tbl);

#endif  // INCLUDE_PB_GPT_H_
