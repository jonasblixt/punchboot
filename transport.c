#include <pb/pb.h>
#include <pb/transport.h>


int pb_transport_init(struct pb_transport *ctx, uint8_t *device_uuid)
{
    ctx->device_uuid = device_uuid;
    return PB_OK;
}

int pb_transport_free(struct pb_transport *ctx)
{
    return PB_OK;
}

int pb_transport_add(struct pb_transport *ctx,
                        struct pb_transport_driver *drv)
{
    drv->device_uuid = ctx->device_uuid;
    ctx->driver = drv;
    return PB_OK;
}

int pb_transport_start(struct pb_transport *ctx)
{
    int rc;

    LOG_DBG("init %p", ctx->driver);
 
    rc = ctx->driver->platform->init(ctx->driver);

    if (rc != PB_OK)
    {
        LOG_ERR("plat failed %i", rc);
        return rc;
    }

    rc = ctx->driver->init(ctx->driver);

    if (rc != PB_OK)
    {
        LOG_ERR("failed %i", rc);
        return rc;
    }

    LOG_DBG("Waiting for driver to become ready");
    while(!ctx->driver->ready)
    {
        ctx->driver->process(ctx->driver);
        plat_wdog_kick();
    }

    LOG_DBG("done");

    return PB_OK;
}

int pb_transport_read(struct pb_transport *ctx,
                      void *buf,
                      size_t size)
{
    return ctx->driver->read(ctx->driver, buf, size);
}

int pb_transport_write(struct pb_transport *ctx,
                      void *buf,
                      size_t size)
{
    return ctx->driver->write(ctx->driver, buf, size);
}

