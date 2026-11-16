#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>

static sc_ipc_t ipc;

void imx8x_rot_helpers_init(sc_ipc_t ipc_)
{
    ipc = ipc_;
}

/* The OEM SRK revocation bits, one per SRK, in the revoke fuse word */
#define IMX8X_OEM_REVOKE_SHIFT (8)
#define IMX8X_OEM_REVOKE_KEYS  (4)

/**
 * Note: On IMX8X there is no direct way of revoking keys. The AHAB
 * Image header must be primed with a key revoke index and then the
 * seco commit command is issued. It revokes what the header says, so
 * this fails unless the key is revoked afterwards.
 */
int imx8x_revoke_key(const struct rot_key *key)
{
    int rc;
    uint32_t before_revoke_bits = 0xffffffff;
    uint32_t after_revoke_bits = 0xffffffff;
    uint32_t key_bit;
    uint32_t info;
    bool fused;

    if (key->slot >= IMX8X_OEM_REVOKE_KEYS) {
        LOG_ERR("Key %u can't be revoked", key->slot);
        return -PB_ERR_PARAM;
    }

    key_bit = 1 << (key->slot + IMX8X_OEM_REVOKE_SHIFT);

    rc = imx8x_is_srk_fused(&fused);

    if (rc != PB_OK)
        return rc;

    if (!fused)
        return -PB_ERR;

    rc = imx8x_fuse_read(IMX8X_FUSE_REVOKE, &before_revoke_bits);

    if (rc != PB_OK) {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    LOG_INFO("Revocation fuse before revocation = %x", before_revoke_bits);

    if (before_revoke_bits & key_bit) {
        LOG_INFO("Key already revoked");
        return PB_OK;
    }

    LOG_INFO("Revoking keys as specified in image header");

#ifdef CONFIG_IMX8X_FUSE_DRY_RUN
    LOG_INFO("Fuse dry run: Would commit the revocation of key %u", key->slot);
    return PB_OK;
#endif

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
    LOG_INFO("Revocation fuse changed bits: %x", before_revoke_bits ^ after_revoke_bits);

    if (!(after_revoke_bits & key_bit)) {
        LOG_ERR("The image header doesn't revoke key %u", key->slot);
        return -PB_ERR_STATE;
    }

    return PB_OK;
}

int imx8x_read_key_status(const struct rot_key *key)
{
    int rc;
    uint32_t current_revoke_bits = 0xffffffff;

    /* AHAB revokes SRK 0 to 3 only. A key in another slot could never be
     * revoked, so it is refused as a revoked one is: no boot, no
     * authentication, and SLC_READ lists it as revoked. v2.7.1 booted it:
     * security before D-008 (D-092, F-429). The build refuses such a key too */
    if (key->slot >= IMX8X_OEM_REVOKE_KEYS) {
        LOG_ERR("Key 0x%x is refused: AHAB can't revoke slot %u", key->id, key->slot);
        return -PB_ERR_KEY_REVOKED;
    }

    rc = imx8x_fuse_read(IMX8X_FUSE_REVOKE, &current_revoke_bits);

    if (rc != PB_OK)
        return rc;

    uint32_t revoke_value = (1u << key->slot);
    uint8_t rom_key_mask = (current_revoke_bits >> IMX8X_OEM_REVOKE_SHIFT) & 0x0f;

    if ((rom_key_mask & revoke_value) == revoke_value)
        return -PB_ERR_KEY_REVOKED;
    else
        return PB_OK;
}

/* The root of trust (platform.md 4.1): AHAB's OEM SRK revocation, through
 * the SECO */
const struct rot_config plat_rot = {
    .revoke_key = imx8x_revoke_key,
    .read_key_status = imx8x_read_key_status,
    .revoke_slots = IMX8X_OEM_REVOKE_KEYS,
};

/* The build's warning for a board key in a slot without a bit reads the
 * makefile.mk's count (platform.md 4.1) */
#ifdef PLAT_REVOKE_SLOTS
_Static_assert(PLAT_REVOKE_SLOTS == IMX8X_OEM_REVOKE_KEYS,
               "makefile.mk's PLAT_REVOKE_SLOTS is AHAB's");
#endif
