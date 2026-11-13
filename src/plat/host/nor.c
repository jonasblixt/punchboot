/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A memory mapped flash for the host platform: a file, mapped read only at a
 * fixed address like a NOR behind a FlexSPI, and written through the bio
 * device. Whatever writes to the map faults, as it would do nothing on a
 * real flash.
 */

#include <errno.h>
#include <fcntl.h>
#include <pb/pb.h>
#include <plat/host/host.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define HOST_NOR_BLOCK_SIZE 512

static int nor_fd = -1;
static const uint8_t *nor_map;
static size_t nor_size;

/* What a driver's callback gets (bio.md 3.1): whole blocks, never none */
static bool whole_blocks(size_t length)
{
    return length != 0 && (length % HOST_NOR_BLOCK_SIZE) == 0;
}

static int host_nor_read(bio_dev_t dev, lba_t lba, size_t length, void *buf)
{
    (void)dev;

    if (!whole_blocks(length))
        return -PB_ERR_PARAM;

    memcpy(buf, &nor_map[(size_t)lba * HOST_NOR_BLOCK_SIZE], length);
    return PB_OK;
}

static int host_nor_write(bio_dev_t dev, lba_t lba, size_t length, const void *buf)
{
    (void)dev;

    if (!whole_blocks(length))
        return -PB_ERR_PARAM;

    ssize_t bytes = pwrite(nor_fd, buf, length, (off_t)lba * HOST_NOR_BLOCK_SIZE);

    if (bytes < 0 || (size_t)bytes != length) {
        LOG_ERR("Write error, lba=%u, length=%zu (%s)", lba, length, strerror(errno));
        return -PB_ERR_IO;
    }

    return PB_OK;
}

int host_nor_map(const char *path, uintptr_t base, size_t size)
{
    uint8_t erased[HOST_NOR_BLOCK_SIZE];
    struct stat st;
    void *map;

    nor_fd = open(path, O_RDWR | O_CREAT, 0644);

    if (nor_fd < 0 || fstat(nor_fd, &st) != 0) {
        LOG_ERR("Could not open '%s' (%s)", path, strerror(errno));
        return -PB_ERR_IO;
    }

    /* A new flash is erased */
    memset(erased, 0xff, sizeof(erased));

    for (off_t offset = st.st_size; (size_t)offset < size; offset += sizeof(erased)) {
        if (pwrite(nor_fd, erased, sizeof(erased), offset) != (ssize_t)sizeof(erased)) {
            LOG_ERR("Could not resize '%s' (%s)", path, strerror(errno));
            return -PB_ERR_IO;
        }
    }

    map = mmap((void *)base, size, PROT_READ, MAP_SHARED | MAP_FIXED_NOREPLACE, nor_fd, 0);

    if (map != (void *)base) {
        LOG_ERR("Could not map '%s' at 0x%lx (%s)", path, (unsigned long)base, strerror(errno));
        return -PB_ERR_MEM;
    }

    nor_map = map;
    nor_size = size;
    return PB_OK;
}

bio_dev_t host_nor_init(const uuid_t uu, const char *description)
{
    int rc;

    if (nor_map == NULL)
        return -PB_ERR_NOT_FOUND;

    bio_dev_t dev =
        bio_allocate(0, nor_size / HOST_NOR_BLOCK_SIZE - 1, HOST_NOR_BLOCK_SIZE, uu, description);

    if (dev < 0)
        return dev;

    rc = bio_set_ios(dev, host_nor_read, host_nor_write);

    if (rc < 0)
        return rc;

    rc = bio_set_flags(dev, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);

    if (rc < 0)
        return rc;

    return dev;
}
