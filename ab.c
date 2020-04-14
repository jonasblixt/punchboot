#include <pb/pb.h>
#include <pb/boot.h>
#include <pb/board.h>
#include <libfdt.h>

#define PB_BOOT_AB_STATE(state) ((struct pb_ab_boot_state *) state->private)

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


static const char *a_part_uu_str  = CONFIG_BOOT_AB_A_UUID;
static const char *b_part_uu_str  = CONFIG_BOOT_AB_B_UUID;

static const char *active_uu_str;
static uint8_t active_uu[16];
static char dt_part_name;

int pb_boot_driver_activate(struct pb_boot_state *state, uint8_t *uu)
{
    struct pb_ab_boot_state *abstate = PB_BOOT_AB_STATE(state);
    char uu_str[37];

    uuid_unparse(uu, uu_str);

    LOG_DBG("Activating: %s", uu_str);

    if (strcmp(uu_str, a_part_uu_str) == 0)
    {
        abstate->enable = PB_STATE_A_ENABLED;
        abstate->verified = PB_STATE_A_VERIFIED;
        abstate->error = 0;
    }
    else if (strcmp(uu_str, b_part_uu_str) == 0)
    {
        abstate->enable = PB_STATE_B_ENABLED;
        abstate->verified = PB_STATE_B_VERIFIED;
        abstate->error = 0;
    }
    else if (strcmp(uu_str, "00000000-0000-0000-0000-000000000000") == 0)
    {
        LOG_INFO("Disable boot partition");
        abstate->enable = 0;
        abstate->verified = 0;
        abstate->error = 0;
    }
    else
    {
        LOG_ERR("Invalid boot partition");
        abstate->enable = 0;
        abstate->verified = 0;
        abstate->error = 0;
        return -PB_ERR;
    }

    return PB_OK;
}


int pb_boot_driver_load_state(struct pb_boot_state *state, bool *commit)
{
    struct pb_ab_boot_state *abstate = PB_BOOT_AB_STATE(state);

    LOG_DBG("A/B boot load state %u %u %u", abstate->enable,
                                            abstate->verified,
                                            abstate->error);

    dt_part_name = '?';

    if (abstate->enable & PB_STATE_A_ENABLED)
    {
        if (!(abstate->verified & PB_STATE_A_VERIFIED) &&
             abstate->remaining_boot_attempts > 0)
        {
            abstate->remaining_boot_attempts--;
            *commit = true;
            active_uu_str = a_part_uu_str;
            dt_part_name = 'A';
        }
        else if (!(abstate->verified & PB_STATE_A_VERIFIED))
        {
            LOG_ERR("Rollback to B system");
            if (!(abstate->verified & PB_STATE_B_VERIFIED))
            {
                LOG_ERR("B system not verified, failing");
                return -PB_ERR;
            }

            active_uu_str = b_part_uu_str;
            dt_part_name = 'B';
        }
        else
        {
            active_uu_str = a_part_uu_str;
            dt_part_name = 'A';
        }
    }
    else if (abstate->enable & PB_STATE_B_ENABLED)
    {
        if (!(abstate->verified & PB_STATE_B_VERIFIED) &&
             abstate->remaining_boot_attempts > 0)
        {
            abstate->remaining_boot_attempts--;
            active_uu_str = b_part_uu_str;
        }
        else if (!(abstate->verified & PB_STATE_B_VERIFIED))
        {
            LOG_ERR("Rollback to A system");
            if (!(abstate->verified & PB_STATE_A_VERIFIED))
            {
                LOG_ERR("A system not verified, failing");
                return -PB_ERR;
            }

            active_uu_str = a_part_uu_str;
            dt_part_name = 'A';
        }
        else
        {
            active_uu_str = b_part_uu_str;
            dt_part_name = 'B';
        }
    }
    else
    {
        active_uu_str = NULL;
        dt_part_name = '?';
    }

    if (!active_uu_str)
    {
        LOG_INFO("No active system");
    }
    else
    {
        uuid_parse(active_uu_str, active_uu);
    }

    return PB_OK;
}

uint8_t *pb_boot_driver_get_part_uu(void)
{
    return active_uu;
}

int pb_boot_driver_boot(int *dtb, int offset)
{

    LOG_DBG("A/B boot");

#ifdef CONFIG_BOOT_DT
    int rc;
    rc = fdt_setprop_string(dtb, offset, "active-system", &dt_part_name);

    if (rc != PB_OK)
    {
        LOG_ERR("Patch dt");
        return rc;
    }
#endif

    return PB_OK;
}

