/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_BIO_H
#define INCLUDE_PB_BIO_H

#include <uuid.h>

struct bio_ops {
    int (*read)(struct bio_device *dev, size_t blocks, size_t off, void *buf);
    int (*write)(struct bio_device *dev, size_t blocks, size_t off, void *buf);
};

struct bio_device {
    uuid_t uu;
    char description[37];
    const struct bio_ops *ops;
    uint64_t first_block;
    uint64_t last_block;
    uint32_t flags;
    uint16_t block_sz;
    bool valid;
};

/**
 * Allocate a new block device structure
 *
 * @return A pointer to a new block device struct or NULL on errors
 */
struct bio_device * bio_allocate_dev(void);
struct bio_device * bio_part_get_by_uu(uuid_t uu);
struct bio_device * bio_part_get_by_idx(unsigned int index);

void bio_flags_set_default(struct bio_device *dev);
void bio_flags_set_bit(struct bio_device *dev, uint16_t flags);

int bio_read(void *buf, size_t n_blocks, size_t offset, struct bio_device *dev);
int bio_write(void *buf, size_t n_blocks, size_t offset, struct bio_device *dev);

uint64_t bio_block_first(struct bio_device *dev);
uint64_t bio_block_last(struct bio_device *dev);
uint64_t bio_block_count(struct bio_device *dev);
uint16_t bio_block_size(struct bio_device *dev);

#endif
