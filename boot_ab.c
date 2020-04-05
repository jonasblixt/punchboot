#include <pb/pb.h>
#include <pb/boot.h>
#include <pb/boot_ab.h>

static int pb_boot_ab_boot(struct pb_boot_driver *boot)
{
    LOG_DBG("A/B boot");
    return -PB_ERR;
}

int pb_boot_ab_init(struct pb_boot_context *ctx, struct pb_boot_driver *boot,
                    struct pb_storage *storage)
{
    struct pb_boot_ab_driver *priv = PB_BOOT_AB_PRIV(boot);

    ctx->driver->boot = pb_boot_ab_boot;

    return pb_boot_init(ctx, boot, storage);
}
