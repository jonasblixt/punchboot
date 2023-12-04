#include <drivers/fuse/imx_ocotp.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <plat/imx6ul/fusebox.h>
#include <plat/imx6ul/hab.h>
#include <plat/imx6ul/imx6ul.h>

int imx6ul_revoke_key(const struct rot_key *key)
{
    int rc;
    uint32_t current_revoke_bits;

    rc = hab_has_no_errors();

    if (rc != PB_OK) {
        LOG_ERR("HAB is reporting errors, aborting");
        return -PB_ERR_STATE;
    }

    rc = imx_ocotp_read(IMX6UL_FUSE_REVOKE_BANK, IMX6UL_FUSE_REVOKE_WORD, &current_revoke_bits);

    if (rc != PB_OK) {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    LOG_DBG("Current revoke value: 0x%x", current_revoke_bits);

    uint32_t revoke_value = (1 << key->param1);

    if ((current_revoke_bits & revoke_value) == revoke_value) {
        LOG_INFO("Key already revoked");
        return PB_OK;
    }

    LOG_INFO("Revoking key '%s' (0x%x)", key->name, key->id);

    revoke_value |= current_revoke_bits;
    LOG_DBG("Updating fuse to 0x%x", revoke_value);

    return imx_ocotp_write(IMX6UL_FUSE_REVOKE_BANK, IMX6UL_FUSE_REVOKE_WORD, revoke_value);
}

int imx6ul_read_key_status(const struct rot_key *key)
{
    int rc;
    uint32_t current_revoke_bits = 0xffffffff;

    rc = imx_ocotp_read(IMX6UL_FUSE_REVOKE_BANK, IMX6UL_FUSE_REVOKE_BANK, &current_revoke_bits);

    if (rc != PB_OK)
        return rc;

    uint32_t revoke_value = (1 << key->param1);

    if ((current_revoke_bits & revoke_value) == revoke_value)
        return -PB_ERR_KEY_REVOKED;
    else
        return PB_OK;
}
