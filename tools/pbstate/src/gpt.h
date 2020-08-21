#ifndef INCLUDE_GPT_H_
#define INCLUDE_GPT_H_

#include <stdint.h>
#include <stdbool.h>

#define GPT_PART_NAME_MAX_SIZE 36

enum
{
    GPT_OK = 0,
    GPT_ERROR = 1,
    GPT_INVALID_HEADER = 2,
    GPT_PART_CRC_ERROR = 3,
    GPT_HEADER_INVALID = 4,
};

#define GPT_HEADER_RSZ 420

struct gpt_header
{
    uint64_t signature;
    uint32_t rev;
    uint32_t hdr_sz;
    uint32_t hdr_crc;
    uint32_t __reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_lba;
    uint64_t last_lba;
    uint8_t disk_uuid[16];
    uint64_t entries_start_lba;
    uint32_t no_of_parts;
    uint32_t part_entry_sz;
    uint32_t part_array_crc;
    uint8_t __reserved2[GPT_HEADER_RSZ];
} __attribute__((packed));

struct gpt_part_hdr
{
    uint8_t type_uuid[16];
    uint8_t uuid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint8_t attr[8];
    uint8_t name[GPT_PART_NAME_MAX_SIZE*2];
} __attribute__((packed));


struct gpt_table
{
    const char *device;
    struct gpt_header hdr;
    struct gpt_part_hdr part[128];
};

#define PB_GPT_ATTR_OK       (1 << 7) /*Bit 55*/

int gpt_init(const char *device, struct gpt_table **gpt);
int gpt_part_by_id(struct gpt_table *gpt, uint8_t id,
                        struct gpt_part_hdr **part);
int gpt_part_by_uuid(struct gpt_table *gpt, const char *uuid,
                        struct gpt_part_hdr **part);
int gpt_free(struct gpt_table *gpt_table);
int gpt_part_name(struct gpt_table *gpt, uint8_t part_index,
                    char *buf, size_t size);

int gpt_get_partname(struct gpt_table *gpt, char *uuid, char *part_name);

#endif  // INCLUDE_GPT_H_
