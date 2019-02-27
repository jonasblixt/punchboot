#ifndef __UUID_H__
#define __UUID_H__

#include <stdint.h>

uint32_t uuid_to_string(uint8_t *uuid, char *out);
uint32_t uuid3_to_string(uint8_t *uuid, char *out);
uint32_t uuid_gen_uuid3(const char *ns,
                        uint32_t ns_length,
                        const char *unique_data,
                        uint32_t unique_data_length,
                        char *out);
#endif
