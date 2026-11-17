/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
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

/*
 * Unlike the OCOTP's driver (F-166) the 8X reads no word back after its
 * write. The SCFW's sc_misc_otp_fuse_read() doesn't say whether it reads the
 * array or a copy the SECO took at reset, and on the 8X the life cycle a
 * forward commits reads the same until a reset: a read back from a copy
 * would fail every write that took. What was written is read after the next
 * reset (slc show).
 */
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

int imx8x_is_srk_fused(bool *fused)
{
    uint32_t val;
    int rc;

    *fused = false;

    for (unsigned int n = 0; n < IMX8X_SRK_HASH_WORDS; n++) {
        rc = imx8x_fuse_read(IMX8X_FUSE_SRK0 + n, &val);

        /* Unknown, not "not fused" */
        if (rc != PB_OK)
            return rc;

        if (val != 0) {
            *fused = true;
            return PB_OK;
        }
    }

    return PB_OK;
}
