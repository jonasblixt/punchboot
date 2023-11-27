#include "api.h"
#include <bpak/bpak.h>
#include <pb-tools/wire.h>
#include <stdlib.h>
#include <string.h>

static int pb_api_debug_stub(struct pb_context *ctx, int level, const char *fmt, ...)
{
    (void)ctx;
    (void)level;
    (void)fmt;
    return PB_RESULT_OK;
}

int pb_api_create_context(struct pb_context **ctxp, pb_debug_t debug)
{
    struct pb_context *ctx = malloc(sizeof(struct pb_context));

    if (!ctx)
        return -PB_RESULT_NO_MEMORY;

    memset(ctx, 0, sizeof(*ctx));

    if (!debug)
        ctx->d = pb_api_debug_stub;
    else
        ctx->d = debug;

    ctx->d(ctx, 2, "%s: init\n", __func__);

    *ctxp = ctx;
    return PB_RESULT_OK;
}

int pb_api_free_context(struct pb_context *ctx)
{
    ctx->d(ctx, 2, "%s: free\n", __func__);

    if (ctx->free)
        ctx->free(ctx);

    free(ctx);
    return PB_RESULT_OK;
}

int pb_api_list_devices(struct pb_context *ctx,
                        void (*list_cb)(const char *uuid_str, void *priv),
                        void *priv)
{
    ctx->d(ctx, 2, "%s: list\n", __func__);
    if (ctx->list)
        return ctx->list(ctx, list_cb, priv);
    return -PB_RESULT_NOT_SUPPORTED;
}
