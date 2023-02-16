#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/boot.h>
#include <pb/storage.h>
#include <uuid.h>

static struct pb_storage_driver *drivers = NULL;

struct pb_storage_driver * pb_storage_get_drivers(void)
{
    return drivers;
}

static int pb_storage_map_init(struct pb_storage_driver *drv)
{
    int rc;

    drv->map_entries = 0;

    if (drv->map_init)
    {
        LOG_DBG("map init");

        struct pb_storage_map *entries = drv->map_data;
        size_t block = 0;

        pb_storage_foreach_part(drv->map_default, p)
        {
            if (p->flags & PB_STORAGE_MAP_FLAG_STATIC_MAP)
            {
                LOG_DBG("Copy static entry %s", p->description);

                if (((p->flags & PB_STORAGE_MAP_FLAG_EMMC_BOOT0) || \
                     (p->flags & PB_STORAGE_MAP_FLAG_EMMC_BOOT1) || \
                     (p->flags & PB_STORAGE_MAP_FLAG_EMMC_RPMB))) {

                    p->first_block = 0;
                    p->last_block = p->no_of_blocks - 1;
                } else if (!p->first_block) {
                    p->first_block = block;
                    p->last_block = block + p->no_of_blocks - 1;
                    block += p->no_of_blocks;
                }

                uuid_parse(p->uuid_str, p->uuid);
                memcpy(&entries[drv->map_entries++], p, sizeof(*p));
            }
        }

        rc = drv->map_init(drv);

        return rc;
    }

    return PB_OK;
}

int pb_storage_early_init(void)
{
    drivers = NULL;
    return PB_OK;
}

int pb_storage_init(void)
{
    int rc = PB_OK;

    LOG_DBG("Storage init <%p>", drivers);

    for (struct pb_storage_driver *drv = drivers; drv; drv = drv->next)
    {
        LOG_INFO("Initializing %s", drv->name);

        if (drv->init)
        {
            rc = drv->init(drv);

            if (rc != PB_OK)
            {
                LOG_ERR("%s init error", drv->name);
                break;
            }
        }

        rc = pb_storage_map_init(drv);

        if (rc != PB_OK)
        {
            LOG_ERR("%s map init err %i", drv->name, rc);
            break;
        }
    }

    LOG_DBG("done %i", rc);

    return rc;
}

int pb_storage_add(struct pb_storage_driver *drv)
{
    if (drivers == NULL)
    {
        drivers = drv;
    }
    else
    {
        drivers->next = drv;
        drivers = drv;
        drv->next = NULL;
    }

    return PB_OK;
}

int pb_storage_read(struct pb_storage_driver *drv,
                    struct pb_storage_map *part,
                    void *buf,
                    size_t blocks,
                    size_t block_offset)
{
    int rc;

    if (blocks > part->no_of_blocks)
    {
        LOG_ERR("blocks > part size");
        return -PB_ERR_MEM;
    }

    if ((block_offset + blocks) > part->no_of_blocks)
    {
        LOG_ERR("Trying to read outside of partition %zu > %llu",
         (block_offset + blocks), part->no_of_blocks);
        return -PB_ERR_MEM;
    }

    rc = drv->read(drv, part->first_block + block_offset, buf, blocks);

    return rc;
}

int pb_storage_write(struct pb_storage_driver *drv,
                    struct pb_storage_map *part,
                    const void *buf,
                    size_t blocks,
                    size_t block_offset)
{
    int rc;

    if (blocks > part->no_of_blocks)
        return -PB_ERR_MEM;
    if ((block_offset + blocks) > part->no_of_blocks)
        return -PB_ERR_MEM;

    rc = drv->write(drv, part->first_block + block_offset, (void *) buf, blocks);

    return rc;
}

int pb_storage_get_part(uint8_t *uuid,
                        struct pb_storage_map **part_out,
                        struct pb_storage_driver **drv_out)
{
    int rc = -PB_ERR;

    for (struct pb_storage_driver *drv = drivers; drv; drv = drv->next)
    {
        rc = -PB_ERR;

        struct pb_storage_map *entries = (struct pb_storage_map *) drv->map_data;

        pb_storage_foreach_part(entries, p)
        {
            if (uuid_compare(uuid, p->uuid) == 0)
            {
                (*part_out) = p;
                (*drv_out) = drv;
                rc = PB_OK;
                goto out;
            }
        }
    }

out:
    return rc;
}

int pb_storage_install_default(void)
{
    int rc;
    LOG_INFO("Installing default");

    for (struct pb_storage_driver *drv = drivers; drv; drv = drv->next)
    {
        if (!drv->map_install)
            continue;

        rc = drv->map_install(drv, \
             (struct pb_storage_map *) drv->map_default);

        if (rc != PB_OK)
            break;

        rc = pb_storage_map_init(drv);

        if (rc != PB_OK)
        {
            LOG_ERR("%s map init err %i", drv->name, rc);
            break;
        }
    }

    if (rc != PB_OK)
        return rc;

    return pb_boot_init();
}

int pb_storage_map(struct pb_storage_driver *drv, struct pb_storage_map **map)
{
    (*map) = (struct pb_storage_map *) drv->map_data;
    return PB_OK;
}

size_t pb_storage_blocks(struct pb_storage_driver *drv)
{
    return (drv->last_block + 1);
}

size_t pb_storage_last_block(struct pb_storage_driver *drv)
{
    return (drv->last_block);
}

int pb_storage_resize(struct pb_storage_driver *drv,
                    struct pb_storage_map *part,
                    size_t blocks)
{
    int rc;

    if (blocks < 1) {
        return -PB_ERR_IO;
    }

    if (drv->map_resize == NULL) {
        return -PB_ERR;
    }

    rc = drv->map_resize(drv, part, blocks);

    if (rc != PB_OK) {
        LOG_ERR("Resize operation failed");
        return rc;
    }

    return pb_storage_map_init(drv);
}
