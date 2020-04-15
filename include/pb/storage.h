#ifndef INCLUDE_PB_STORAGE_H_
#define INCLUDE_PB_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * \def PB_PART_FLAG_BOOTABLE
 * The partition is bootable
 *
 * \def PB_PART_FLAG_OTP
 * The partition can only be written once
 *
 * \def PB_PART_FLAG_WRITABLE
 * The partition is writeable
 *
 * \def PB_PART_FLAG_ERASE_BEFORE_WRITE
 * The partition must be erased before any write operation
 */

#define PB_STORAGE_MAP_FLAG_BOOTABLE (1 << 0)
#define PB_STORAGE_MAP_FLAG_OTP      (1 << 1)
#define PB_STORAGE_MAP_FLAG_WRITABLE (1 << 2)
#define PB_STORAGE_MAP_FLAG_ERASE_BEFORE_WRITE (1 << 3)
#define PB_STORAGE_MAP_FLAG_DISK_MAP (1 << 4)
#define PB_STORAGE_MAP_FLAG_VISIBLE (1 << 5)


#define PB_STORAGE_MAP_FLAG_STATIC_MAP (1 << 16)
#define PB_STORAGE_MAP_FLAG_EMMC_BOOT0  (1 << 17)
#define PB_STORAGE_MAP_FLAG_EMMC_BOOT1  (1 << 18)

struct pb_storage_map
{
    bool valid_entry;
    char uuid_str[37];
    uint8_t uuid[16];     /*!< Partition UUID */
    char description[37]; /*!< Textual description of partition */
    uint64_t first_block; /*!< Partition start block */
    uint64_t last_block;  /*!< Last(inclusive) block of partition */
    uint64_t no_of_blocks;
    int flags;            /*!< Flags */
};

#define pb_storage_foreach_part(__tbl, __part) \
    for (struct pb_storage_map *__part = (struct pb_storage_map *)__tbl; \
                __part->valid_entry; __part++)

#define PB_STORAGE_MAP(__uuid_str, __desc, __blks, __flags) \
    { \
        .valid_entry = true, \
        .uuid_str = __uuid_str, \
        .uuid = "", \
        .description = __desc, \
        .first_block = 0, \
        .last_block = 0, \
        .no_of_blocks = __blks, \
        .flags = __flags, \
    }

#define PB_STORAGE_MAP2(__uuid_str, __uuid_bytes, __desc, __blks, __flags) \
    { \
        .valid_entry = true, \
        .uuid_str = __uuid_str, \
        .uuid = __uuid_bytes, \
        .description = __desc, \
        .first_block = 0, \
        .last_block = 0, \
        .no_of_blocks = __blks, \
        .flags = __flags, \
    }

#define PB_STORAGE_MAP3(__uuid_str, __desc, __start_blk, __end_blk, __flags) \
    { \
        .valid_entry = true, \
        .uuid_str = __uuid_str, \
        .uuid = "", \
        .description = __desc, \
        .first_block = __start_blk, \
        .last_block = __end_blk, \
        .no_of_blocks = (__end_blk - __start_blk + 1), \
        .flags = __flags, \
    }
#define PB_STORAGE_MAP_END \
    { \
        .valid_entry = false, \
        .uuid = "", \
        .uuid_str = "", \
        .description = "", \
        .first_block = 0, \
        .last_block = 0, \
        .no_of_blocks = 0, \
        .flags = 0, \
    }

struct pb_storage_driver;

typedef int (*pb_storage_io_t) (struct pb_storage_driver *drv,
                                size_t block_offset,
                                void *buf,
                                size_t n_blocks);

typedef int (*pb_storage_call_t) (struct pb_storage_driver *drv);

typedef int (*pb_storage_map_t) (struct pb_storage_driver *drv,
                                  struct pb_storage_map *map);

struct pb_storage_driver
{
    bool ready;
    const char *name;
    const size_t block_size;
    int last_block;
    pb_storage_call_t init;
    pb_storage_io_t write;
    pb_storage_io_t read;
    pb_storage_io_t flush;
    const void *driver_private;
    /* Partition map interface */
    pb_storage_call_t map_init;
    pb_storage_map_t map_install;
    pb_storage_map_t map_request;
    pb_storage_map_t map_release;
    const struct pb_storage_map *map_default;
    void *map_private;
    size_t map_private_size;
    void *map_data;
    size_t map_data_size;
    unsigned int map_entries;
    struct pb_storage_driver *next;
};

int pb_storage_early_init(void);
int pb_storage_init(void);

int pb_storage_add(struct pb_storage_driver *drv);

int pb_storage_read(struct pb_storage_driver *drv,
                    struct pb_storage_map *part,
                    void *buf,
                    size_t blocks,
                    size_t block_offset);

int pb_storage_write(struct pb_storage_driver *drv,
                    struct pb_storage_map *part,
                    const void *buf,
                    size_t blocks,
                    size_t block_offset);

int pb_storage_get_part(uint8_t *uuid,
                        struct pb_storage_map **part,
                        struct pb_storage_driver **drv);

int pb_storage_install_default(void);
int pb_storage_map(struct pb_storage_driver *drv, struct pb_storage_map **map);
size_t pb_storage_blocks(struct pb_storage_driver *drv);
size_t pb_storage_last_block(struct pb_storage_driver *drv);
struct pb_storage_driver * pb_storage_get_drivers(void);

#endif  // INCLUDE_PB_STORAGE_H_
