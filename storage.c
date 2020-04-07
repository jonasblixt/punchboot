#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/storage.h>
#include <uuid/uuid.h>

int pb_storage_init(struct pb_storage *ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    return PB_OK;
}

int pb_storage_free(struct pb_storage *ctx)
{
    return PB_OK;
}

int pb_storage_add(struct pb_storage *ctx, struct pb_storage_driver *drv)
{
    if ((ctx->no_of_drivers + 1) > PB_STORAGE_MAX_DRIVERS)
        return -PB_ERR_MEM;

    ctx->drivers[ctx->no_of_drivers++] = drv;

    return PB_OK;
}

int pb_storage_start(struct pb_storage *ctx)
{
    int rc = PB_OK;
    struct pb_storage_driver *drv;

    for (int i = 0 ;i < ctx->no_of_drivers; i++)
    {
        drv = ctx->drivers[i];

        LOG_INFO("Initializing %s", drv->name);

        if (drv->platform)
        {
            rc = drv->platform->init(drv);

            if (rc != PB_OK)
            {
                LOG_ERR("%s platform error", drv->name);
                break;
            }
        }

        if (drv->init)
        {
            rc = drv->init(drv);

            if (rc != PB_OK)
            {
                LOG_ERR("%s init error", drv->name);
                break;
            }
        }

        if (drv->map)
        {
            LOG_DBG("map init %p", drv->map);

            struct pb_storage_map *entries = drv->map->map_data;
            drv->map->map_entries = 0;
            size_t block = 0;

            pb_storage_foreach_part(drv->default_map, p)
            {
                if (p->flags & PB_STORAGE_MAP_FLAG_STATIC_MAP)
                {
                    LOG_DBG("Copy static entry %s", p->description);

                    p->first_block = block;
                    p->last_block = block + p->no_of_blocks - 1;
                    block += p->no_of_blocks;
                    uuid_parse(p->uuid_str, p->uuid);

                    memcpy(&entries[drv->map->map_entries++], p, sizeof(*p));
                }
            }

            rc = drv->map->init(drv);

            if (rc != PB_OK)
            {
                LOG_ERR("%s map init err %i\n", drv->name, rc);
                break;
            }
        }
    }

    LOG_DBG("done %i", rc);

    return rc;
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
        LOG_ERR("Trying to read outside of partition %lu > %llu",
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

int pb_storage_get_part(struct pb_storage *ctx,
                        uuid_t uu,
                        struct pb_storage_map **part,
                        struct pb_storage_driver **driver)
{
    int rc;

    for (int i = 0; i < ctx->no_of_drivers; i++)
    {
        struct pb_storage_driver *drv = ctx->drivers[i];
        const struct pb_storage_map_driver *map_driver = drv->map;
        const struct pb_storage_map *map = NULL;

        if (map_driver)
            map = map_driver->map;
        else
            map = drv->default_map;

        rc = -PB_ERR;

        pb_storage_foreach_part(map, p)
        {
            if (uuid_compare(uu, p->uuid) == 0)
            {
                (*part) = p;
                (*driver) = drv;
                rc = PB_OK;
                goto out;
            }
        }
    }

out:
    return rc;
}

int pb_storage_install_default(struct pb_storage *ctx)
{
    int rc;
    LOG_INFO("Installing default");

    for (int i = 0; i < ctx->no_of_drivers; i++)
    {
        struct pb_storage_driver *drv = ctx->drivers[i];
        const struct pb_storage_map_driver *map_driver = drv->map;

        if (!map_driver)
            continue;
        if (!map_driver->install)
            continue;

        rc = map_driver->install(drv, \
             (struct pb_storage_map *) drv->default_map);

        if (rc != PB_OK)
            break;
    }

    return rc;
}

int pb_storage_map(struct pb_storage_driver *drv, struct pb_storage_map **map)
{

    if (drv->map)
    {
        if (drv->map->map)
            (*map) = drv->map->map;
    }
    else if (drv->default_map)
    {
        (*map) = (struct pb_storage_map *) drv->default_map;
    }
    else
    {
        return -PB_ERR;
    }

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
