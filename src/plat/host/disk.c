/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <pb/pb.h>
#include <plat/host/host.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define HOST_DISK_BLOCK_SIZE 512

static int disk_fd = -1;

static int host_disk_read(bio_dev_t dev, lba_t lba, size_t length, void *buf)
{
    (void)dev;
    ssize_t bytes = pread(disk_fd, buf, length, (off_t)lba * HOST_DISK_BLOCK_SIZE);

    if (bytes < 0 || (size_t)bytes != length) {
        LOG_ERR("Read error, lba=%u, length=%zu (%s)", lba, length, strerror(errno));
        return -PB_ERR_IO;
    }

    return PB_OK;
}

static int host_disk_write(bio_dev_t dev, lba_t lba, size_t length, const void *buf)
{
    (void)dev;
    ssize_t bytes = pwrite(disk_fd, buf, length, (off_t)lba * HOST_DISK_BLOCK_SIZE);

    if (bytes < 0 || (size_t)bytes != length) {
        LOG_ERR("Write error, lba=%u, length=%zu (%s)", lba, length, strerror(errno));
        return -PB_ERR_IO;
    }

    return PB_OK;
}

bio_dev_t host_disk_init(const char *path, size_t size, const uuid_t uu, const char *description)
{
    int rc;
    struct stat st;

    disk_fd = open(path, O_RDWR | O_CREAT, 0644);

    if (disk_fd < 0) {
        LOG_ERR("Could not open '%s' (%s)", path, strerror(errno));
        return -PB_ERR_IO;
    }

    if (fstat(disk_fd, &st) != 0 || (size_t)st.st_size < size) {
        if (ftruncate(disk_fd, (off_t)size) != 0) {
            LOG_ERR("Could not resize '%s' (%s)", path, strerror(errno));
            return -PB_ERR_IO;
        }
    }

    bio_dev_t dev =
        bio_allocate(0, size / HOST_DISK_BLOCK_SIZE - 1, HOST_DISK_BLOCK_SIZE, uu, description);

    if (dev < 0)
        return dev;

    rc = bio_set_ios(dev, host_disk_read, host_disk_write);

    if (rc < 0)
        return rc;

    rc = bio_set_flags(dev, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);

    if (rc < 0)
        return rc;

    return dev;
}
