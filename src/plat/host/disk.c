/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Block devices backed by files: the disk, and the fuses' file (plat.c).
 * Each device's file is its descriptor, in the device's private data, which
 * its partitions inherit.
 */

#include <errno.h>
#include <fcntl.h>
#include <pb/pb.h>
#include <plat/host/host.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define HOST_DISK_BLOCK_SIZE 512

/* The files: the disk and the fuses */
#define HOST_DISKS           2

/* The files that are open, by the UUID of the device that reads each */
static struct {
    bool used;
    uuid_t uu;
    int fd;
} disks[HOST_DISKS];

/* What a driver's callback gets (bio.md 3.1): whole blocks, never none. A
 * file would take any length, and hide a caller that a real driver fails */
static bool whole_blocks(size_t length)
{
    return length != 0 && (length % HOST_DISK_BLOCK_SIZE) == 0;
}

static int disk_fd(bio_dev_t dev)
{
    return (int)bio_get_private(dev);
}

static int host_disk_read(bio_dev_t dev, lba_t lba, size_t length, void *buf)
{
    if (!whole_blocks(length))
        return -PB_ERR_PARAM;

    ssize_t bytes = pread(disk_fd(dev), buf, length, (off_t)lba * HOST_DISK_BLOCK_SIZE);

    if (bytes < 0 || (size_t)bytes != length) {
        LOG_ERR("Read error, lba=%u, length=%zu (%s)", lba, length, strerror(errno));
        return -PB_ERR_IO;
    }

    return PB_OK;
}

static int host_disk_write(bio_dev_t dev, lba_t lba, size_t length, const void *buf)
{
    if (!whole_blocks(length))
        return -PB_ERR_PARAM;

    ssize_t bytes = pwrite(disk_fd(dev), buf, length, (off_t)lba * HOST_DISK_BLOCK_SIZE);

    if (bytes < 0 || (size_t)bytes != length) {
        LOG_ERR("Write error, lba=%u, length=%zu (%s)", lba, length, strerror(errno));
        return -PB_ERR_IO;
    }

    return PB_OK;
}

/* No descriptor is kept that no device reads: not after a failed init, and
 * not the first file after a second init of the same device (F-406) */
static void disk_close(unsigned int n)
{
    if (disks[n].used)
        close(disks[n].fd);

    disks[n].used = false;
}

/* The entry of the device 'uu': its own when it was set up before, which a
 * second init replaces, else a free one. -1 when there is none */
static int disk_entry(const uuid_t uu)
{
    int free_entry = -1;

    for (unsigned int n = 0; n < HOST_DISKS; n++) {
        if (disks[n].used && uuid_compare(disks[n].uu, uu) == 0) {
            disk_close(n);
            return (int)n;
        }

        if (!disks[n].used && free_entry < 0)
            free_entry = (int)n;
    }

    return free_entry;
}

bio_dev_t host_disk_init(const char *path, size_t size, const uuid_t uu, const char *description)
{
    int rc;
    struct stat st;
    int n = disk_entry(uu);

    if (n < 0) {
        LOG_ERR("No room for another disk");
        return -PB_ERR_MEM;
    }

    disks[n].fd = open(path, O_RDWR | O_CREAT, 0644);

    if (disks[n].fd < 0) {
        LOG_ERR("Could not open '%s' (%s)", path, strerror(errno));
        return -PB_ERR_IO;
    }

    disks[n].used = true;
    uuid_copy(disks[n].uu, uu);

    if (fstat(disks[n].fd, &st) != 0 || (size_t)st.st_size < size) {
        if (ftruncate(disks[n].fd, (off_t)size) != 0) {
            LOG_ERR("Could not resize '%s' (%s)", path, strerror(errno));
            disk_close(n);
            return -PB_ERR_IO;
        }
    }

    bio_dev_t dev =
        bio_allocate(0, size / HOST_DISK_BLOCK_SIZE - 1, HOST_DISK_BLOCK_SIZE, uu, description);

    if (dev < 0) {
        disk_close(n);
        return dev;
    }

    rc = bio_set_ios(dev, host_disk_read, host_disk_write);

    if (rc == PB_OK)
        rc = bio_set_private(dev, (uintptr_t)disks[n].fd);

    if (rc == PB_OK)
        rc = bio_set_flags(dev, BIO_FLAG_VISIBLE | BIO_FLAG_WRITABLE);

    if (rc < 0) {
        disk_close(n);
        return rc;
    }

    return dev;
}
