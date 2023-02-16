/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/fuse.h>
#include <pb/pb.h>
#include <pb/storage.h>
#include <plat/qemu/fuse.h>
#include <plat/qemu/virtio.h>
#include <plat/qemu/virtio_block.h>
#include <uuid.h>

#define QEMU_FUSE_MAX 128

static uint32_t _fuse_box[QEMU_FUSE_MAX];
static struct pb_storage_driver *sdrv = NULL;
static struct pb_storage_map *map = NULL;

int qemu_fuse_init(void)
{
    uint8_t fusebox_uu[16];
    int rc;

    LOG_INFO("Initializing fuse array");

    uuid_parse("44acdcbe-dcb0-4d89-b0ad-8f96967f8c95", fusebox_uu);

    rc = pb_storage_get_part(fusebox_uu, &map, &sdrv);

    if (rc != PB_OK)
    {
        LOG_ERR("Fusebox init error");
        return rc;
    }

    rc = pb_storage_read(sdrv, map, _fuse_box, 1, 0);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not read fuse array");
        return rc;
    }

    LOG_INFO("Fuse array initialized <%p, %p>", sdrv, map);

    return rc;
}

int qemu_fuse_write(uint32_t id, uint32_t val)
{
    if (id >= QEMU_FUSE_MAX)
        return PB_ERR;

    _fuse_box[id] |= val;

    LOG_DBG("_fuse_box[%u] = %x", id, _fuse_box[id]);
    LOG_DBG("sdrv = %p, map = %p", sdrv, map);
    return pb_storage_write(sdrv, map, _fuse_box, 1, 0);
}


int qemu_fuse_read(uint32_t id, uint32_t *val)
{
    if (id >= QEMU_FUSE_MAX)
        return -PB_ERR;

    *val = _fuse_box[id];

    return PB_OK;
}
