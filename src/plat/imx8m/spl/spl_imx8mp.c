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
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/ramlog.h>
#include <stddef.h>
#include <string.h>

#include <plat/imx8m/spl.h>

#include "../plat.h"
#include "ivt.h"

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

/*
 * HAB's vector table at 0x900 on the 8M Plus, 64-bit entries (U-Boot,
 * arch/arm/include/asm/mach-imx/hab.h; plat-imx.md 2, the HAB log)
 */
#define IMX8MP_HAB_RVT        0x900
#define HAB_RVT_ENTRY         0x08
#define HAB_RVT_EXIT          0x10
#define HAB_RVT_CHECK_TARGET  0x18
#define HAB_RVT_AUTHENTICATE  0x20
#define HAB_RVT_REPORT_STATUS 0x48
#define HAB_SUCCESS           0xf0
#define HAB_CFG_OPEN          0xf0
#define HAB_TGT_MEMORY        0x0f
#define HAB_CID_UBOOT         1 /* The caller id U-Boot gives HAB */

typedef uint32_t (*hab_entry_t)(void);
typedef uint32_t (*hab_exit_t)(void);
typedef uint32_t (*hab_check_target_t)(uint32_t type, const void *start, size_t bytes);
typedef void *(*hab_authenticate_t)(uint8_t cid,
                                    ptrdiff_t ivt_offset,
                                    void **start,
                                    size_t *bytes,
                                    void *loader);
typedef uint32_t (*hab_report_status_t)(uint8_t *config, uint8_t *state);

/* A pointer the ROM keeps at addr. The ROM is at 0: the compiler takes a
 * constant address there for NULL, so it doesn't see one */
static uintptr_t rom_entry(uintptr_t addr)
{
    __asm__("" : "+r"(addr));
    return (uintptr_t)mmio_read_64(addr);
}

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

/* pb proper's header, from the boot device at its offset, or from the USB
 * stream, read page by page until a page starts with it. The image's
 * first page, the header's, is at the load address (hdr.boot_data.start,
 * the IVT's own) afterwards */
static int load(struct imx_header *hdr, const struct imx_ivt_limits *limits)
{
    uint8_t *scratch = (uint8_t *)(uintptr_t)PB_BUFFERS_BASE;
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

/* HAB authenticates pb proper where it is, its IVT at its start. On an open
 * device a failure is HAB's events only, as the ROM's own (U-Boot's
 * imx_hab_authenticate_image()) */
static int authenticate(const struct imx_header *hdr)
{
    hab_entry_t hab_entry = (hab_entry_t)rom_entry(IMX8MP_HAB_RVT + HAB_RVT_ENTRY);
    hab_exit_t hab_exit = (hab_exit_t)rom_entry(IMX8MP_HAB_RVT + HAB_RVT_EXIT);
    hab_check_target_t check_target =
        (hab_check_target_t)rom_entry(IMX8MP_HAB_RVT + HAB_RVT_CHECK_TARGET);
    hab_authenticate_t hab_authenticate =
        (hab_authenticate_t)rom_entry(IMX8MP_HAB_RVT + HAB_RVT_AUTHENTICATE);
    hab_report_status_t report_status =
        (hab_report_status_t)rom_entry(IMX8MP_HAB_RVT + HAB_RVT_REPORT_STATUS);
    void *start = (void *)(uintptr_t)hdr->boot_data.start;
    size_t bytes = hdr->boot_data.size;
    uint8_t config = 0;
    uint8_t state = 0;
    void *loaded = NULL;

    if (hab_entry() == HAB_SUCCESS && check_target(HAB_TGT_MEMORY, start, bytes) == HAB_SUCCESS)
        loaded = hab_authenticate(HAB_CID_UBOOT, 0, &start, &bytes, NULL);

    if (hab_exit() != HAB_SUCCESS)
        loaded = NULL;

    (void)report_status(&config, &state);

    if (loaded == NULL) {
        LOG_ERR("HAB: pb proper not authenticated (config %02x, state %02x)", config, state);

        if (config != HAB_CFG_OPEN)
            return -PB_ERR_SIGNATURE;
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
        authenticate(&hdr) == PB_OK) {
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
