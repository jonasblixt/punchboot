/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M Plus's SPL (D-088, plat-imx.md 2). Its v2 ROM takes no
 * plugin: the SPL brings up DRAM, reads pb proper with the ROM's API,
 * has HAB authenticate it and jumps to it. pb proper is IMX8MP_PB_OFFSET
 * bytes into the image, its header first (ivt.h).
 */

#include <arch/arch_helpers.h>
#include <pb/dma.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/ramlog.h>
#include <plat/imx8m/imx8m.h>
#include <stddef.h>
#include <string.h>

#include <plat/imx8m/spl.h>

#include "../plat.h"
#include "ivt.h"
#include "rom.h"

/*
 * The ROM's API at 0x980 (U-Boot, arch/arm/mach-imx/imx8m/soc.c,
 * g_rom_api; romapi.c): download_image() reads from the boot device at an
 * offset or, over USB, the next bytes of the stream; query_boot_infor()
 * answers what the ROM booted from. Each takes its arguments XORed into the
 * last one.
 */
#define IMX8MP_ROM_API  0x980
#define ROM_API_OKAY    0xf0
#define QUERY_BT_DEV    2
#define QUERY_PAGE_SZ   3
#define QUERY_IMG_OFF   6
#define BT_DEV_TYPE_USB 0xe

struct rom_api {
    uint16_t ver;
    uint16_t tag;
    uint32_t reserved1;
    uint32_t (*download_image)(uint8_t *dest, uint32_t offset, uint32_t size, uint32_t xor);
    uint32_t (*query_boot_infor)(uint32_t info_type, uint32_t *info, uint32_t xor);
};

static const struct rom_api *rom_api(void)
{
    uintptr_t addr = IMX8MP_ROM_API;

    __asm__("" : "+r"(addr));
    return (const struct rom_api *)addr;
}

static uint32_t rom_download(void *dest, uint32_t offset, uint32_t size)
{
    uint32_t xor = (uint32_t)(uintptr_t)dest ^ offset ^ size;

    return rom_api()->download_image(dest, offset, size, xor);
}

static uint32_t rom_query(uint32_t type, uint32_t *info)
{
    return rom_api()->query_boot_infor(type, info, type ^ (uint32_t)(uintptr_t)info);
}

/* A page the ROM's API reads into before pb proper's place is known: in
 * DRAM, at the start of pb's buffers (spl/link.lds) */
PB_DMA_ARRAY(uint8_t, scratch, 0x1000);

/* pb proper's header, from the boot device at its offset, or from the USB
 * stream, read page by page until a page starts with it. The image's
 * first page, the header's, is at the load address (hdr.boot_data.start,
 * the IVT's own) afterwards */
static int load(struct imx_header *hdr, const struct imx_ivt_limits *limits)
{
    uint32_t dev = 0;
    uint32_t page = 0;
    uint32_t image_offset = 0;
    bool stream;
    uint32_t rest;

    if (rom_query(QUERY_BT_DEV, &dev) != ROM_API_OKAY ||
        rom_query(QUERY_PAGE_SZ, &page) != ROM_API_OKAY || page == 0 || page > 0x1000) {
        LOG_ERR("ROM API: boot device %08x, page %u", dev, page);
        return -PB_ERR_IO;
    }

    stream = ((dev >> 16) & 0xff) == BT_DEV_TYPE_USB;

    if (!stream && rom_query(QUERY_IMG_OFF, &image_offset) != ROM_API_OKAY) {
        LOG_ERR("ROM API: no image offset");
        return -PB_ERR_IO;
    }

    LOG_INFO("ROM API: device %08x, page %u, image at %u", dev, page, image_offset);

    if (stream) {
        /* What the ROM hasn't read of the SPL's part, then pb proper's
         * header at a page's start: at most the SPL's part in pages */
        for (unsigned int n = 0;; n++) {
            if (n > IMX8MP_PB_OFFSET / page || rom_download(scratch, 0, page) != ROM_API_OKAY) {
                LOG_ERR("ROM API: no pb proper in the stream");
                return -PB_ERR_IO;
            }

            memcpy(hdr, scratch, sizeof(*hdr));

            if ((hdr->ivt.header & 0xff) == IMX_IVT_TAG && hdr->ivt.entry == PB_ENTRY)
                break;
        }
    } else if (rom_download(scratch, image_offset + IMX8MP_PB_OFFSET, page) != ROM_API_OKAY) {
        LOG_ERR("ROM API: reading pb proper's header failed");
        return -PB_ERR_IO;
    } else {
        memcpy(hdr, scratch, sizeof(*hdr));
    }

    if (imx_ivt_check(hdr, limits) != PB_OK)
        return -PB_ERR_PARAM;

    /* The build makes the size a multiple of 4 KiB, the largest page */
    if (hdr->boot_data.size % page != 0 || hdr->boot_data.start != hdr->ivt.self) {
        LOG_ERR("pb proper: %u bytes at %08x, page %u",
                hdr->boot_data.size,
                hdr->boot_data.start,
                page);
        return -PB_ERR_PARAM;
    }

    memcpy((void *)(uintptr_t)hdr->boot_data.start, scratch, page);
    rest = hdr->boot_data.size - page;

    if (rest > 0 && rom_download((void *)(uintptr_t)(hdr->boot_data.start + page),
                                 stream ? 0 : image_offset + IMX8MP_PB_OFFSET + page,
                                 rest) != ROM_API_OKAY) {
        LOG_ERR("ROM API: reading pb proper failed");
        return -PB_ERR_IO;
    }

    return PB_OK;
}

void spl_main(void);

void spl_main(void)
{
    const struct imx_ivt_limits limits = {
        .entry = PB_ENTRY,
        .region_end = PB_ENTRY + PB_SIZE,
        .load_floor = PB_BUFFERS_BASE + PB_BUFFERS_SIZE,
    };
    struct imx_header hdr;

#ifdef CONFIG_RAMLOG
    ramlog_init();
#endif
    imx8m_spl_console_init();
    LOG_INFO("SPL " PB_VERSION);

    if (imx8m_spl_dram_init() == PB_OK && load(&hdr, &limits) == PB_OK &&
        imx8m_spl_hab_authenticate(IMX8MP_HAB_RVT_BASE, &hdr) == PB_OK) {
        LOG_INFO("pb proper: %08x", hdr.ivt.entry);
        ((void (*)(void))(uintptr_t)hdr.ivt.entry)();
    }

    /* Nothing to go back to: the v2 ROM doesn't take the SPL's return. A
     * platform failure resets (platform.md 2); should it not, the watchdog,
     * if the ROM armed it, does */
    LOG_ERR("SPL failed, resetting");
    imx8m_spl_reset();

    while (true)
        wfi();
}
