/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/misc/api.h>
#include <plat/imx8x/sci/svc/pm/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>

static sc_ipc_t ipc;

void imx8x_fuse_init(sc_ipc_t ipc_)
{
    ipc = ipc_;
}

int imx8x_fuse_write(uint32_t addr, uint32_t value)
{
    sc_err_t err;

    if (ipc <= 0)
        return -PB_ERR;

#ifdef CONFIG_IMX8X_FUSE_DRY_RUN
    (void)err;
    LOG_INFO("Fuse dry run: Would write 0x%x to %i", value, addr);
    return PB_OK;
#else
    err = sc_misc_otp_fuse_write(ipc, addr, value);
    if (err == SC_ERR_NONE) {
        return PB_OK;
    } else {
        LOG_ERR("Writing fuse %i failed (%i)", addr, err);
        return -PB_ERR_IO;
    }
#endif
}

int imx8x_fuse_read(uint32_t addr, uint32_t *result)
{
    sc_err_t err;

    if (ipc <= 0)
        return -PB_ERR;

    err = sc_misc_otp_fuse_read(ipc, addr, result);

    if (err == SC_ERR_NONE) {
        return PB_OK;
    } else {
        LOG_ERR("Reading fuse %i failed (%i)", addr, err);
        return -PB_ERR_IO;
    }
}

int imx8x_fuse_write_srk(const struct imx8x_srk_fuses *fuses)
{
    int rc;
    LOG_INFO("Fusing SRK fuses");

    if (fuses == NULL)
        return -PB_ERR_PARAM;

    /* Check for empty SRK fields */
    for (unsigned int n = 0; n < 16; n++) {
        if (fuses->srk[n] == 0)
            return -PB_ERR_PARAM;
    }

    for (unsigned int n = 0; n < 16; n++) {
        LOG_INFO("Fusing SRK%i, value=0x%x", n, fuses->srk[n]);
        rc = imx8x_fuse_write(730 + n, fuses->srk[n]);

        if (rc != PB_OK)
            return rc;
    }

    return PB_OK;
}

bool imx8x_is_srk_fused(void)
{
    uint32_t val;

    for (unsigned int n = 0; n < 16; n++) {
        (void)imx8x_fuse_read(730 + n, &val);

        if (val != 0)
            return true;
    }

    return false;
}
