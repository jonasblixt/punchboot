/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "ramdisk.h"
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

static struct ramdisk *disk_from_dev(bio_dev_t dev)
{
    return (struct ramdisk *)bio_get_private(dev);
}

static int ramdisk_read(bio_dev_t dev, lba_t lba, size_t length, void *buf)
{
    struct ramdisk *disk = disk_from_dev(dev);

    if (((uint64_t)lba * RAMDISK_BLOCK_SIZE + length) > (uint64_t)disk->blocks * RAMDISK_BLOCK_SIZE)
        return -PB_ERR_IO;

    memcpy(buf, ramdisk_block(disk, lba), length);
    disk->reads++;
    return PB_OK;
}

static int ramdisk_write(bio_dev_t dev, lba_t lba, size_t length, const void *buf)
{
    struct ramdisk *disk = disk_from_dev(dev);

    if (((uint64_t)lba * RAMDISK_BLOCK_SIZE + length) > (uint64_t)disk->blocks * RAMDISK_BLOCK_SIZE)
        return -PB_ERR_IO;

    memcpy(ramdisk_block(disk, lba), buf, length);
    disk->writes++;
    return PB_OK;
}

struct ramdisk *ramdisk_new(size_t blocks)
{
    struct ramdisk *disk = calloc(1, sizeof(*disk));

    if (disk == NULL)
        return NULL;

    disk->data = calloc(blocks, RAMDISK_BLOCK_SIZE);

    if (disk->data == NULL) {
        free(disk);
        return NULL;
    }

    disk->blocks = blocks;
    return disk;
}

void ramdisk_free(struct ramdisk *disk)
{
    if (disk == NULL)
        return;
    free(disk->data);
    free(disk);
}

bio_dev_t ramdisk_attach(struct ramdisk *disk, const uuid_t uu)
{
    int rc;
    bio_dev_t dev = bio_allocate(0, disk->blocks - 1, RAMDISK_BLOCK_SIZE, uu, "ramdisk");

    if (dev < 0)
        return dev;

    rc = bio_set_ios(dev, ramdisk_read, ramdisk_write);

    if (rc != PB_OK)
        return rc;

    rc = bio_set_private(dev, (uintptr_t)disk);

    if (rc != PB_OK)
        return rc;

    rc = bio_set_flags(dev, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);

    if (rc != PB_OK)
        return rc;

    return dev;
}

uint8_t *ramdisk_block(struct ramdisk *disk, lba_t lba)
{
    return &disk->data[(size_t)lba * RAMDISK_BLOCK_SIZE];
}
