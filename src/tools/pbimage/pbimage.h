#ifndef __PBIMAGE_H__
#define __PBIMAGE_H__

#include <stdint.h>
#include <stdbool.h>


uint32_t pbimage_prepare(uint32_t key_index, uint32_t key_mask,
                         const char *key_source,
                         const char *output_fn);

uint32_t pbimage_append_component(const char *comp_type,
                                  uint32_t load_addr,
                                  const char *fn);

uint32_t pbimage_out(const char *fn);

#endif
