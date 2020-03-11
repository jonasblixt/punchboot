#include <stdlib.h>
#include <string.h>
#include <pb/api.h>
#include <pb/protocol.h>

static int pb_read_resultcode(struct pb_context *ctx)
{
    int rc;
    uint32_t result = -1;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_OK)
        return rc;

    if (result)
        return -PB_ERR;

    return PB_OK;
}

int pb_create_context(struct pb_context **ctxp)
{
    struct pb_context *ctx = malloc(sizeof(struct pb_context));

    if (!ctx)
        return -PB_NO_MEMORY;

    memset(ctx, 0, sizeof(*ctx));

    *ctxp = ctx;
    return PB_OK;
}

int pb_free_context(struct pb_context *ctx)
{
    if (ctx->free)
        ctx->free(ctx);

    free(ctx);
    return PB_OK;
}

int pb_get_version(struct pb_context *ctx, char *output, size_t sz)
{
    char version[64];
    int rc;
    uint32_t result;
    uint32_t read_sz = 0;

    rc = ctx->command(ctx, PB_CMD_BL_VERSION, 0, 0, 0, 0, NULL, 0);

    if (rc != PB_OK)
        return rc;

    rc = ctx->read(ctx, &read_sz, sizeof(read_sz));

    if (rc != PB_OK)
        return rc;

    if (read_sz > sz)
        read_sz = sz;

    rc = ctx->read(ctx, version, read_sz);

    if (rc != PB_OK)
        return rc;

    memcpy(output, version, read_sz);


    return pb_read_resultcode(ctx);
}

int pb_device_reset(struct pb_context *ctx)
{
    int rc;

    rc = ctx->command(ctx, PB_CMD_RESET, 0, 0, 0, 0, NULL, 0);

    if (rc != PB_OK)
        return rc;

    return pb_read_resultcode(ctx);
}
