#ifndef INCLUDE_PB_BOOT_AB_H_
#define INCLUDE_PB_BOOT_AB_H_

#include <pb/boot.h>
#include <pb/storage.h>

#define PB_BOOT_AB_PRIV(boot) ((struct pb_boot_ab_driver *) boot->private)
#define PB_BOOT_AB_STATE(boot) ((struct pb_ab_boot_state *) boot->state->private)

#define PB_STATE_A_ENABLED (1 << 0)
#define PB_STATE_B_ENABLED (1 << 1)
#define PB_STATE_A_VERIFIED (1 << 0)
#define PB_STATE_B_VERIFIED (1 << 1)
#define PB_STATE_ERROR_A_ROLLBACK (1 << 0)
#define PB_STATE_ERROR_B_ROLLBACK (1 << 1)

struct pb_ab_boot_state
{
    uint32_t enable;
    uint32_t verified;
    uint32_t remaining_boot_attempts;
    uint32_t error;
} __attribute__((packed));

struct pb_ab_boot_pair
{
    const char *name;
    const char *image;
    void *board_private;
};

struct pb_boot_ab_driver
{
    const struct pb_ab_boot_pair a;
    const struct pb_ab_boot_pair b;
    struct pb_ab_boot_pair *active;
};

int pb_boot_ab_init(struct pb_boot_context *ctx, struct pb_boot_driver *boot,
                    struct pb_storage *storage,
                    struct pb_crypto *crypto,
                    struct bpak_keystore *keystore);

#endif
