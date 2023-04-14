#include <pb/pb.h>
#include <plat/imx6ul/imx6ul.h>
#include <plat/imx6ul/fusebox.h>
#include <drivers/fuse/imx_ocotp.h>

#define IMX6UL_FUSE_SRK_BANK (3)

int imx6ul_fuse_write_srk(const struct imx6ul_srk_fuses *fuses)
{
    int rc;
    uint32_t val;

    LOG_INFO("Fusing SRK fuses");

    if (fuses == NULL)
        return -PB_ERR_PARAM;

    /* Check for empty SRK fields */
    for (unsigned int n = 0; n < 8; n++) {
        if (fuses->srk[n] == 0)
            return -PB_ERR_PARAM;
    }

    /* Already fused? */
    bool perform_srk_fusing = false;

    for (unsigned int n = 0; n < 8; n++) {
        rc = imx_ocotp_read(IMX6UL_FUSE_SRK_BANK, n, &val);

        if (rc != PB_OK)
            return rc;

        /* Check if we're trying to change an already fused SRK */
        if (val != 0 && val != fuses->srk[n]) {
            LOG_ERR("Trying to change already fused srk[%u] from 0x%x to 0x%x",
                    n, val, fuses->srk[n]);
            return -PB_ERR_MEM;
        } else if (val != 0 && val == fuses->srk[n]) {
            LOG_INFO("srk[%u] already fused to 0x%x", n, val);
        } else {
            perform_srk_fusing = true;
        }
    }

    if (perform_srk_fusing) {
        LOG_INFO("Writing SRK...");

        for (unsigned int n = 0; n < 8; n++) {
            LOG_INFO("Fusing srk[%u] to 0x%x", n, fuses->srk[n]);
            rc = imx_ocotp_write(IMX6UL_FUSE_SRK_BANK, n, fuses->srk[n]);

            if (rc != PB_OK)
                return rc;
        }
    }

    return PB_OK;
}

bool imx6ul_is_srk_fused(void)
{
    int rc;
    uint32_t fuse_val;

    for (unsigned int n = 0; n < 8; n++) {
        rc = imx_ocotp_read(IMX6UL_FUSE_SRK_BANK, n, &fuse_val);

        if (rc != PB_OK)
            return false;

        if (fuse_val != 0)
            return true;
    }

    return false;
}
