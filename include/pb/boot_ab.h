#ifndef INCLUDE_PB_BOOT_AB_H_
#define INCLUDE_PB_BOOT_AB_H_

#include <pb/boot.h>
#include <pb/storage.h>

#define PB_BOOT_AB_PRIV(boot) ((struct pb_boot_ab_driver *) boot->private)

struct pb_ab_boot_pair
{
    const char *system;
    const char *root;
};

struct pb_boot_ab_driver
{
    const struct pb_ab_boot_pair a;
    const struct pb_ab_boot_pair b;
    struct pb_ab_boot_pair *active;
};

int pb_boot_ab_init(struct pb_boot_context *ctx, struct pb_boot_driver *boot,
                    struct pb_storage *storage);

#endif
