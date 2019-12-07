/**
 * Punch BOOT bootloader cli
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <stdio.h>
#include "utils.h"


void utils_gpt_part_name(struct gpt_part_hdr *part, uint8_t *out, uint8_t len) {
     uint8_t null_count = 0;

     for (int i = 0; i < len*2; i++) {
         if (part->name[i]) {
             *out++ = part->name[i];
             null_count = 0;
         } else {
             null_count++;
         }

         *out = 0;

         if (null_count > 1)
             break;
     }
}

