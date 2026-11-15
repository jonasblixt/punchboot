/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pb/pb.h>

#include "ivt.h"

int imx_ivt_check(const struct imx_header *hdr, const struct imx_ivt_limits *limits)
{
    const struct imx_ivt *ivt = &hdr->ivt;
    const struct imx_boot_data *bd = &hdr->boot_data;
    uint32_t tag = ivt->header & 0xff;
    uint32_t length = ((ivt->header >> 16) & 0xff) | (ivt->header & 0xff00);
    uint32_t version = ivt->header >> 24;
    uint64_t load_end = (uint64_t)bd->start + bd->size;

    if (tag != IMX_IVT_TAG || length != IMX_IVT_LENGTH || version < IMX_IVT_VERSION_MIN ||
        version > IMX_IVT_VERSION_MAX) {
        LOG_ERR("pb proper's IVT: header %08x", ivt->header);
        return -PB_ERR_PARAM;
    }

    if (ivt->entry != limits->entry || ivt->self != limits->entry - IMX_HEADER_SIZE ||
        ivt->boot_data != ivt->self + sizeof(struct imx_ivt) || ivt->dcd != 0 || bd->plugin != 0) {
        LOG_ERR("pb proper's IVT: entry %08x, self %08x, boot data %08x, DCD %08x, plugin %u",
                ivt->entry,
                ivt->self,
                ivt->boot_data,
                ivt->dcd,
                bd->plugin);
        return -PB_ERR_PARAM;
    }

    if (bd->size == 0 || bd->start < limits->load_floor || bd->start > ivt->self ||
        load_end > limits->region_end || ivt->csf < ivt->self + IMX_HEADER_SIZE ||
        ivt->csf >= load_end) {
        LOG_ERR("pb proper's load: %08x, %u bytes, CSF %08x", bd->start, bd->size, ivt->csf);
        return -PB_ERR_PARAM;
    }

    return PB_OK;
}
