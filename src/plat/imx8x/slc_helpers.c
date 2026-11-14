#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/slc.h>
#include <pb/srk_fuse.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>

static sc_ipc_t ipc;

/* SECO life cycle bits 6-10: closed, closed with NXP firmware, the two
 * field return states and no return. Each of them is past OEM closed. */
#define IMX8X_LC_CLOSED        (0x7c0)

/* sc_seco_forward_lifecycle()'s transition to OEM closed */
#define IMX8X_LC_TO_OEM_CLOSED (16)

void imx8x_slc_helpers_init(sc_ipc_t ipc_)
{
    ipc = ipc_;
}

slc_t imx8x_slc_read_status(void)
{
    uint16_t lc;
    uint16_t monotonic;
    uint32_t uid_l;
    uint32_t uid_h;
    bool fused;
    int rc;

    if (sc_seco_chip_info(ipc, &lc, &monotonic, &uid_l, &uid_h) != SC_ERR_NONE) {
        LOG_ERR("Could not read the life cycle");
        return -PB_ERR_IO;
    }

    if (lc & IMX8X_LC_CLOSED) {
        return SLC_CONFIGURATION_LOCKED;
    }

    /**
     * Note: If SRK fuses are set or partially set we
     * report 'SLC_CONFIGURATION'. This is not perfect but will probably
     * cover most cases. An SRK word that can't be read leaves the life cycle
     * unknown (security.md 4.4).
     */
    rc = imx8x_is_srk_fused(&fused);

    if (rc != PB_OK)
        return rc;

    if (fused)
        return SLC_CONFIGURATION;
    else
        return SLC_NOT_CONFIGURED;
}

int imx8x_slc_set_configuration(void)
{
    return imx8x_fuse_write_srk(srk_fuse, srk_fuse_words);
}

/* slc_set_configuration_locked() checks the state first: CONFIGURATION,
 * so the SRK hash is fused and the chip isn't closed */
int imx8x_slc_set_configuration_locked(void)
{
    LOG_INFO("About to change security state to locked");

#ifdef CONFIG_IMX8X_FUSE_DRY_RUN
    LOG_INFO("Fuse dry run: Would forward the life cycle to OEM closed");
    return PB_OK;
#else
    if (sc_seco_forward_lifecycle(ipc, IMX8X_LC_TO_OEM_CLOSED) != SC_ERR_NONE)
        return -PB_ERR_IO;

    return PB_OK;
#endif
}

/*
 * The life cycle (platform.md 4.2): the SECO's, and the fuses through the
 * SCFW. No EOL (F-263).
 *
 * The i.MX8DXL has no set_configuration_locked: the build can't sign pb for
 * it yet (imx8xl_sign_image in its board's makefile.mk), and a closed DXL
 * would refuse the only image there is. 'slc lock' answers NOT_SUPPORTED
 * until signing exists (F-212).
 */
const struct slc_ops plat_slc = {
    .read_status = imx8x_slc_read_status,
    .set_configuration = imx8x_slc_set_configuration,
#ifndef CONFIG_PLAT_IMX8XL
    .set_configuration_locked = imx8x_slc_set_configuration_locked,
#endif
    .fuse_read = imx8x_fuse_read,
    .fuse_write = imx8x_fuse_write,
};
