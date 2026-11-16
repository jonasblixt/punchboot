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
#include <pb_build_version.h>
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

/* What the ROM's API reads before pb proper's place is known: in DRAM, at
 * the start of pb's buffers (spl/link.lds). Over USB the rest of the SPL's
 * part and pb proper's header, so the SPL's part and a page */
#define SCRATCH_SIZE (IMX8MP_PB_OFFSET + 0x1000)
PB_DMA_ARRAY(uint8_t, scratch, SCRATCH_SIZE);

/* pb proper's header at 'p': its IVT's tag, length and version word as
 * gen-imx8m-ivt.py writes it, and the entry the SPL was built for */
static bool is_header(const uint8_t *p)
{
    struct imx_ivt ivt;

    memcpy(&ivt, p, sizeof(ivt));
    return (ivt.header & 0xff) == IMX_IVT_TAG && ivt.entry == PB_ENTRY &&
           ivt.self == PB_ENTRY - IMX_HEADER_SIZE;
}

/*
 * Over USB the ROM's API streams: each download_image() is the next bytes
 * the host sends. The ROM has read the SPL's image and whatever it reads
 * ahead of it, so where pb proper's header falls in the stream isn't known.
 * The SPL reads page after page and looks for it at every word, as U-Boot's
 * spl_romapi.c looks for its FIT. It returns the header's offset in
 * 'scratch' and the bytes read after it, or -PB_ERR_IO.
 *
 * The ROM answers a page of 1 byte over USB (read on the 8MP EVK), and a
 * read of 1 byte at an odd address fails: the stream is read in pieces of
 * at least STREAM_CHUNK, as U-Boot's 1 KiB.
 */
#define STREAM_CHUNK 1024

static int find_in_stream(uint32_t page, size_t *at, size_t *have)
{
    size_t read = 0;
    size_t next = 0;

    page = MAX(page, (uint32_t)STREAM_CHUNK);

    while (read + page <= SCRATCH_SIZE) {
        if (rom_download(scratch + read, 0, page) != ROM_API_OKAY)
            break;

        read += page;

        for (; next + IMX_HEADER_SIZE <= read; next += 4) {
            if (is_header(scratch + next)) {
                *at = next;
                *have = read - next;
                return PB_OK;
            }
        }
    }

    LOG_ERR("ROM API: no pb proper in %u bytes of the stream", (unsigned int)read);
    return -PB_ERR_IO;
}

/* pb proper, its header first: from the boot device at its offset, or the
 * next of the USB stream. Afterwards pb proper is at its load address
 * (hdr.boot_data.start, the IVT's own) */
static int load(struct imx_header *hdr, const struct imx_ivt_limits *limits)
{
    uint32_t dev = 0;
    uint32_t page = 0;
    uint32_t image_offset = 0;
    size_t at = 0;
    size_t have;
    bool stream;
    uint8_t *dest;

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
        if (find_in_stream(page, &at, &have) != PB_OK)
            return -PB_ERR_IO;

        LOG_DBG("ROM API: pb proper's header %u bytes into the stream's pages", (unsigned int)at);
    } else if (rom_download(scratch, image_offset + IMX8MP_PB_OFFSET, page) != ROM_API_OKAY) {
        LOG_ERR("ROM API: reading pb proper's header failed");
        return -PB_ERR_IO;
    } else {
        have = page;
    }

    memcpy(hdr, scratch + at, sizeof(*hdr));

    if (imx_ivt_check(hdr, limits) != PB_OK)
        return -PB_ERR_PARAM;

    /* The build makes the size a multiple of 4 KiB, the largest page: the
     * boot device is read in whole pages */
    if (hdr->boot_data.size % page != 0 || hdr->boot_data.start != hdr->ivt.self) {
        LOG_ERR("pb proper: %u bytes at %08x, page %u",
                hdr->boot_data.size,
                hdr->boot_data.start,
                page);
        return -PB_ERR_PARAM;
    }

    /* What was read after the header is pb proper's, as much of it as it
     * has: the stream goes on with nothing else */
    have = MIN(have, (size_t)hdr->boot_data.size);
    dest = (uint8_t *)(uintptr_t)hdr->boot_data.start;
    memcpy(dest, scratch + at, have);

    if (have < hdr->boot_data.size &&
        rom_download(dest + have,
                     stream ? 0 : image_offset + IMX8MP_PB_OFFSET + (uint32_t)have,
                     hdr->boot_data.size - (uint32_t)have) != ROM_API_OKAY) {
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
    LOG_INFO("SPL " PB_BUILD_VERSION);

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
