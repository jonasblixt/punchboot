#include <pb/pb.h>
#include <pb/crypto.h>

int pb_crypto_init(struct pb_crypto *crypto)
{
    crypto->drivers = NULL;
    return PB_OK;
}

int pb_crypto_free(struct pb_crypto *crypto)
{
    return PB_OK;
}

int pb_crypto_add(struct pb_crypto *crypto, struct pb_crypto_driver *drv)
{
    if (crypto->drivers == NULL)
        crypto->drivers = drv;
    else
    {
        struct pb_crypto_driver *d = crypto->drivers;

        while (d->next)
            d = d->next;

        d->next = drv;
    }

    return PB_OK;
}

int pb_crypto_start(struct pb_crypto *crypto)
{
    struct pb_crypto_driver *drv = (struct pb_crypto_driver *) \
                                   crypto->drivers;
    int rc;

    if (!drv)
        return -PB_ERR;

    if (drv->platform)
    {
        LOG_DBG("crypto plat init");
        rc = drv->platform->init(drv);

        if (rc != PB_OK)
            return rc;
    }

    if (drv->init)
       rc = drv->init(drv);

    return rc;
}

int pb_hash_init(struct pb_crypto *crypto, struct pb_hash_context *ctx,
                        enum pb_hash_algs alg)
{
    ctx->driver = crypto->drivers;
    ctx->alg = alg;

    return ctx->driver->hash_init(ctx->driver, ctx, alg);
}

int pb_hash_update(struct pb_hash_context *ctx, void *buf, size_t size)
{
    struct pb_crypto_driver *drv = ctx->driver;
    return drv->hash_update(drv, ctx, buf, size);
}

int pb_hash_finalize(struct pb_hash_context *ctx, void *buf, size_t size)
{

    struct pb_crypto_driver *drv = ctx->driver;
    return drv->hash_final(drv, ctx, buf, size);
}

int pb_pk_verify(struct pb_crypto *crypto, void *signature, size_t size,
                    struct pb_hash_context *hash, struct bpak_key *key)
{
    struct pb_crypto_driver *drv = crypto->drivers;
    return drv->pk_verify(drv, hash, key, signature, size);
}
