#include <pb/pb.h>
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
     * cover most cases.
     */
    if (imx8x_is_srk_fused())
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

    if (sc_seco_forward_lifecycle(ipc, IMX8X_LC_TO_OEM_CLOSED) != SC_ERR_NONE)
        return -PB_ERR_IO;

    return PB_OK;
}
