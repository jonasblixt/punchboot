#include <stdlib.h>
#include <string.h>
#include <pb-tools/pb-tools.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>
#include <bpak/bpak.h>

static int pb_api_debug_stub(struct pb_context *ctx, int level,
                                    const char *fmt, ...)
{
    (void) ctx;
    (void) level;
    (void) fmt;
    return PB_RESULT_OK;
}


PB_EXPORT int pb_api_create_context(struct pb_context **ctxp, pb_debug_t debug)
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

PB_EXPORT int pb_api_free_context(struct pb_context *ctx)
{
    ctx->d(ctx, 2, "%s: free\n", __func__);

    if (ctx->free)
        ctx->free(ctx);

    free(ctx);
    return PB_RESULT_OK;
}



