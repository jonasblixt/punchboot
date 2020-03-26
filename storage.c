#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/storage.h>

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
                    p->last_block = p->no_of_blocks - 1;
                    block += p->no_of_blocks;

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

int pb_storage_read(struct pb_storage *ctx,
                    struct pb_storage_map *part,
                    void *buf,
                    size_t blocks,
                    size_t block_offset)
{
    return PB_OK;
}

int pb_storage_write(struct pb_storage *ctx,
                    struct pb_storage_map *part,
                    const void *buf,
                    size_t blocks,
                    size_t block_offset)
{
    return PB_OK;
}

int pb_storage_get_part(struct pb_storage *ctx,
                        uint8_t *uuid,
                        struct pb_storage_map **part)
{
    return PB_OK;
}

int pb_storage_install_default(struct pb_storage *ctx)
{
    return PB_OK;
}

int pb_storage_map(struct pb_storage *ctx, struct pb_storage_map *map)
{
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
