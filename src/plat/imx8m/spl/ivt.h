/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * HABv4's image vector table and boot data, as imx-mkimage writes them
 * (8MQ RM 6.1.6.1 and 6.1.6.2; mkimage_imx8.c's imx_header_v2_t, 0x40
 * bytes: the IVT, the boot data and padding).
 */

#ifndef PLAT_IMX8M_SPL_IVT_H
#define PLAT_IMX8M_SPL_IVT_H

#include <stdint.h>

#define IMX_IVT_TAG          0xd1
#define IMX_IVT_LENGTH       0x20 /* Big endian in the header */
#define IMX_IVT_VERSION_MIN  0x40
#define IMX_IVT_VERSION_MAX  0x4f
#define IMX_HEADER_SIZE      0x40 /* imx_header_v2_t */
#define IMX_BOOT_DATA_PLUGIN 1u

struct imx_ivt {
    uint32_t header; /* Tag, length (big endian), version */
    uint32_t entry;
    uint32_t reserved1;
    uint32_t dcd;
    uint32_t boot_data;
    uint32_t self;
    uint32_t csf;
    uint32_t reserved2;
};

struct imx_boot_data {
    uint32_t start;
    uint32_t size;
    uint32_t plugin;
};

struct imx_header {
    struct imx_ivt ivt;
    struct imx_boot_data boot_data;
    uint32_t padding[5];
};

_Static_assert(sizeof(struct imx_header) == IMX_HEADER_SIZE, "imx_header_v2_t is 0x40 bytes");

/* What the SPL was built for: pb proper's entry, the end of its region and
 * the lowest address the ROM may load to, the end of pb's buffers */
struct imx_ivt_limits {
    uint32_t entry;
    uint32_t region_end;
    uint32_t load_floor;
};

/**
 * Check pb proper's header against the SPL's build (D-088, plat-imx.md 2),
 * before anything of it is used: the IVT's tag, length and version, its entry
 * and address, the boot data right after it, no DCD and no plugin flag, and
 * the load range and the CSF inside [limits->load_floor, limits->region_end)
 * with the IVT in it.
 *
 * @param[in] hdr The header, a copy the caller keeps and uses from here on
 * @param[in] limits The build's values
 *
 * @return PB_OK when it may be loaded, -PB_ERR_PARAM otherwise
 */
int imx_ivt_check(const struct imx_header *hdr, const struct imx_ivt_limits *limits);

#endif // PLAT_IMX8M_SPL_IVT_H
