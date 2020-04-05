#ifndef INCLUDE_PB_STORAGE_H_
#define INCLUDE_PB_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>

#define PB_STORAGE_MAX_DRIVERS 8

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
    char description[16]; /*!< Textual description of partition */
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
struct pb_storage_map_driver;

typedef int (*pb_storage_io_t) (struct pb_storage_driver *drv,
                                size_t block_offset,
                                void *buf,
                                size_t n_blocks);

typedef int (*pb_storage_call_t) (struct pb_storage_driver *drv);

typedef int (*pb_storage_map_t) (struct pb_storage_driver *drv,
                                  struct pb_storage_map *map);

struct pb_storage_map_driver
{
    pb_storage_call_t init;
    pb_storage_call_t load;
    pb_storage_map_t install;
    struct pb_storage_map *map;
    void *private;
    size_t size;
    void *map_data;
    size_t map_size;
    int map_entries;
};

struct pb_storage_platform_driver
{
    pb_storage_call_t init;
    pb_storage_call_t free;
    void *private;
    size_t size;
};

struct pb_storage_driver
{
    bool ready;
    const char *name;
    const size_t block_size;
    int last_block;
    pb_storage_call_t init;
    pb_storage_call_t free;
    pb_storage_io_t write;
    pb_storage_io_t read;
    pb_storage_io_t flush;
    pb_storage_map_t map_request;
    pb_storage_map_t map_release;
    const struct pb_storage_map *default_map;
    struct pb_storage_map_driver *map;
    struct pb_storage_platform_driver *platform;
    const void *private;
};

struct pb_storage
{
    int no_of_drivers;
    struct pb_storage_driver *drivers[PB_STORAGE_MAX_DRIVERS];
};

int pb_storage_init(struct pb_storage *ctx);

int pb_storage_add(struct pb_storage *ctx, struct pb_storage_driver *drv);

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

int pb_storage_get_part(struct pb_storage *ctx,
                        uint8_t *uuid,
                        struct pb_storage_map **part,
                        struct pb_storage_driver **drv);

int pb_storage_start(struct pb_storage *ctx);
int pb_storage_install_default(struct pb_storage *ctx);
int pb_storage_free(struct pb_storage *ctx);

int pb_storage_map(struct pb_storage_driver *drv, struct pb_storage_map **map);
size_t pb_storage_blocks(struct pb_storage_driver *drv);

size_t pb_storage_last_block(struct pb_storage_driver *drv);

#endif  // INCLUDE_PB_STORAGE_H_
