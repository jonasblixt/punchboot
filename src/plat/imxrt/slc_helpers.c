#include <pb/pb.h>
#include <pb/slc.h>
#include <drivers/fuse/imx_ocotp.h>
#include <plat/imxrt/imxrt.h>

static bool imxrt_is_srk_fused(void)
{
    int rc;
    uint32_t fuse_val;

    for (unsigned int n = 0; n < 8; n++) {
        rc = imx_ocotp_read(3, n, &fuse_val);

        if (rc != PB_OK)
            return false;

        if (fuse_val != 0)
            return true;
    }

    return false;
}

slc_t imxrt_slc_read_status(void)
{
    int rc;
    uint32_t tmp;
    bool sec_boot_active = false;

    rc = imx_ocotp_read(0, 6, &tmp);
    if (rc != PB_OK) {
        return rc;
    }

    sec_boot_active = !!(tmp & 0x2);

    if (sec_boot_active) {
        return SLC_CONFIGURATION_LOCKED;
    }

    /**
     * Note: If SRK fuses are set or partially set we
     * report 'SLC_CONFIGURATION'. This is not perfect but will probably
     * cover most cases.
     */
    if (imxrt_is_srk_fused())
        return SLC_CONFIGURATION;
    else
        return SLC_NOT_CONFIGURED;
}

int imxrt_slc_set_configuration_locked(void)
{
        return -PB_ERR_STATE;
}
