#include <stdio.h>
#include "utils.h"


void utils_gpt_part_name(struct gpt_part_hdr *part, u8 *out, u8 len) {
     u8 null_count = 0;

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

