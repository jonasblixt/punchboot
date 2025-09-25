#include <pb/pb.h>
#include <pb/slc.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>

static sc_ipc_t ipc;

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

    sc_seco_chip_info(ipc, &lc, &monotonic, &uid_l, &uid_h);

    if (lc == 128) {
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

int imx8x_slc_set_configuration_locked(void)
{
    int err;
    uint16_t lc;
    uint16_t monotonic;
    uint32_t uid_l;
    uint32_t uid_h;

    if (!imx8x_is_srk_fused()) {
        LOG_ERR("SRK is empty, refusing to advance life cycle");
        return -PB_ERR_STATE;
    }

    sc_seco_chip_info(ipc, &lc, &monotonic, &uid_l, &uid_h);

    if (lc == 128) {
        LOG_INFO("Configuration already locked");
        return PB_OK;
    }

    LOG_INFO("About to change security state to locked");

    err = sc_seco_forward_lifecycle(ipc, 16);

    if (err != SC_ERR_NONE)
        return -PB_ERR_IO;

    return PB_OK;
}
