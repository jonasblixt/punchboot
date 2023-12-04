#include <pb/pb.h>
#include <pb/rot.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/pm/sci_pm_api.h>
#include <plat/imx8x/sci/svc/seco/sci_seco_api.h>

static sc_ipc_t ipc;

void imx8x_rot_helpers_init(sc_ipc_t ipc_)
{
    ipc = ipc_;
}

/**
 * Note: On IMX8X there is no direct way of revoking keys. The AHAB
 * Image header must be primed with a key revoke index and then the
 * seco commit command is issued.
 */
int imx8x_revoke_key(const struct rot_key *key)
{
    int rc;
    uint32_t before_revoke_bits = 0xffffffff;
    uint32_t after_revoke_bits = 0xffffffff;
    uint32_t info;

    (void)key;

    if (!imx8x_is_srk_fused())
        return -PB_ERR;

    LOG_INFO("Revoking keys as specified in image header");

    rc = imx8x_fuse_read(IMX8X_FUSE_REVOKE, &before_revoke_bits);

    if (rc != PB_OK) {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    LOG_INFO("Revocation fuse before revocation = %x", after_revoke_bits);

    /* Commit OEM revocations = 0x10 */
    info = 0x10;

    /* sc_seco_commit returns which resource was revoked in info. In
     * our case, info should be 0x10 for OEM key after the revocation
     * is done. */
    rc = sc_seco_commit(ipc, &info);
    if (rc != SC_ERR_NONE) {
        LOG_ERR("sc_seco_commit failed: %i", rc);
        return -PB_ERR_IO;
    }

    LOG_INFO("Commit reply: %x", info);

    rc = imx8x_fuse_read(IMX8X_FUSE_REVOKE, &after_revoke_bits);
    if (rc != PB_OK) {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    LOG_INFO("Revocation fuse after revocation = %x", after_revoke_bits);

    if (before_revoke_bits == after_revoke_bits) {
        LOG_INFO("The revocation fuse had the same value before and after revocation");
        return PB_OK;
    }

    LOG_INFO("Revocation fuse changed bits: %x", before_revoke_bits ^ after_revoke_bits);

    return PB_OK;
}

int imx8x_read_key_status(const struct rot_key *key)
{
    int rc;
    uint32_t current_revoke_bits = 0xffffffff;

    rc = imx8x_fuse_read(IMX8X_FUSE_REVOKE, &current_revoke_bits);

    if (rc != PB_OK)
        return rc;

    uint32_t revoke_value = (1 << key->param1);
    uint8_t rom_key_mask = (current_revoke_bits >> 8) & 0x0f;

    if ((rom_key_mask & revoke_value) == revoke_value)
        return -PB_ERR_KEY_REVOKED;
    else
        return PB_OK;
}
