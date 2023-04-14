#include <pb/pb.h>
#include <pb/slc.h>
#include <plat/imx6ul/imx6ul.h>
#include <plat/imx6ul/fusebox.h>
#include <plat/imx6ul/hab.h>
#include <drivers/fuse/imx_ocotp.h>

slc_t imx6ul_slc_read_status(void)
{
    int rc;
    bool sec_boot_active = false;

    rc = hab_secureboot_active(&sec_boot_active);

    if (rc != PB_OK)
        return rc;

    if (sec_boot_active) {
        return SLC_CONFIGURATION_LOCKED;
    }

    /**
     * Note: If SRK fuses are set or partially set we
     * report 'SLC_CONFIGURATION'. This is not perfect but will probably
     * cover most cases.
     */
    if (imx6ul_is_srk_fused())
        return SLC_CONFIGURATION;
    else
        return SLC_NOT_CONFIGURED;
}

int imx6ul_slc_set_configuration_locked(void)
{
    slc_t slc;

    if (!imx6ul_is_srk_fused()) {
        LOG_ERR("SRK is empty, refusing to advance life cycle");
        return -PB_ERR_MEM;
    }

    slc = imx6ul_slc_read_status();

    if (slc < 0)
        return slc;

    if (slc == SLC_CONFIGURATION_LOCKED) {
        LOG_INFO("Configuration already locked");
        return PB_OK;
    } else if (slc != SLC_CONFIGURATION) {
        LOG_ERR("SLC is not in configuration, aborting (%u)", slc);
        return -PB_ERR_STATE;
    }

    LOG_INFO("About to change security state to locked");

    uint32_t lock_value = 0x02;

    return imx_ocotp_write(IMX6UL_FUSE_LOCK_BANK,
                           IMX6UL_FUSE_LOCK_WORD,
                           lock_value);
}
