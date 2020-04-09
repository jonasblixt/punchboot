#include <pb/pb.h>
#include <pb/boot.h>
#include <pb/boot_ab.h>
#include <libfdt.h>


static int activate_part(struct pb_boot_driver *boot, uint8_t *uu)
{
    struct pb_boot_ab_driver *priv = PB_BOOT_AB_PRIV(boot);
    struct pb_ab_boot_state *state = PB_BOOT_AB_STATE(boot);
    char uu_str[37];

    boot->update_boot_state = true;
    uuid_unparse(uu, uu_str);

    LOG_DBG("Activating: %s", uu_str);

    if (strcmp(uu_str, priv->a.image) == 0)
    {
        priv->active = (struct pb_ab_boot_pair *) &priv->a;
        state->enable = PB_STATE_A_ENABLED;
        state->verified = PB_STATE_A_VERIFIED;
        state->error = 0;
    }
    else if (strcmp(uu_str, priv->b.image) == 0)
    {
        priv->active = (struct pb_ab_boot_pair *) &priv->b;
        state->enable = PB_STATE_B_ENABLED;
        state->verified = PB_STATE_B_VERIFIED;
        state->error = 0;
    }
    else if (strcmp(uu_str, "00000000-0000-0000-0000-000000000000") == 0)
    {
        LOG_ERR("Disable boot partition");
        state->enable = 0;
        state->verified = 0;
        state->error = 0;
    }
    else
    {
        LOG_ERR("Invalid boot partition");
        state->enable = 0;
        state->verified = 0;
        state->error = 0;
        return -PB_ERR;
    }

    return PB_OK;
}

static int pb_boot_ab_load_state(struct pb_boot_driver *boot)
{
    struct pb_boot_ab_driver *priv = PB_BOOT_AB_PRIV(boot);
    struct pb_ab_boot_state *state = PB_BOOT_AB_STATE(boot);

    LOG_DBG("A/B boot load state %u %u %u", state->enable,
                                            state->verified,
                                            state->error);

    if (state->enable & PB_STATE_A_ENABLED)
    {
        if (!(state->verified & PB_STATE_A_VERIFIED) &&
             state->remaining_boot_attempts > 0)
        {
            state->remaining_boot_attempts--;
            boot->update_boot_state = true;
            priv->active = (struct pb_ab_boot_pair *) &priv->a;
        }
        else if (!(state->verified & PB_STATE_A_VERIFIED))
        {
            LOG_ERR("Rollback to B system");
            if (!(state->verified & PB_STATE_B_VERIFIED))
            {
                LOG_ERR("B system not verified, failing");
                return -PB_ERR;
            }

            priv->active = (struct pb_ab_boot_pair *) &priv->b;
        }
        else
        {
            priv->active = (struct pb_ab_boot_pair *) &priv->a;
        }
    }
    else if (state->enable & PB_STATE_B_ENABLED)
    {
        if (!(state->verified & PB_STATE_B_VERIFIED) &&
             state->remaining_boot_attempts > 0)
        {
            state->remaining_boot_attempts--;
            boot->update_boot_state = true;
            priv->active = (struct pb_ab_boot_pair *) &priv->b;
        }
        else if (!(state->verified & PB_STATE_B_VERIFIED))
        {
            LOG_ERR("Rollback to A system");
            if (!(state->verified & PB_STATE_A_VERIFIED))
            {
                LOG_ERR("A system not verified, failing");
                return -PB_ERR;
            }

            priv->active = (struct pb_ab_boot_pair *) &priv->a;
        }
        else
        {
            priv->active = (struct pb_ab_boot_pair *) &priv->b;
        }
    }
    else
    {
        priv->active = NULL;
    }

    if (!priv->active)
    {
        LOG_INFO("No active system");
    }
    else
    {
        uuid_parse(priv->active->image, boot->boot_part_uu);
    }

    return PB_OK;
}

static int pb_boot_ab_boot(struct pb_boot_driver *boot)
{

    LOG_DBG("A/B boot");
    boot->on_jump(boot);
    return -PB_ERR;
}

static int pb_boot_ab_patch_dt(struct pb_boot_driver *boot, void *dtb,
                int offset)
{
    struct pb_boot_ab_driver *priv = PB_BOOT_AB_PRIV(boot);
    const char *name;

    if (!priv->active)
        name = "?";
    else
        name = priv->active->name;

    return fdt_setprop_string(dtb, offset, "active-system", name);
}

int pb_boot_ab_init(struct pb_boot_context *ctx, struct pb_boot_driver *boot,
                    struct pb_storage *storage,
                    struct pb_crypto *crypto,
                    struct bpak_keystore *keystore)
{
    struct pb_boot_driver *drv = boot;
    struct pb_boot_ab_driver *priv = PB_BOOT_AB_PRIV(boot);

    priv->active = NULL;

    drv->boot = pb_boot_ab_boot;
    drv->load_boot_state = pb_boot_ab_load_state;

    if (boot->dtb_image_id)
        drv->patch_dt = pb_boot_ab_patch_dt;
    else
        drv->patch_dt = NULL;

    drv->activate = activate_part;

    return pb_boot_init(ctx, boot, storage, crypto, keystore);
}
