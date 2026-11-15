/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8MQ's SPL, a HABv4 plugin (8MQ RM 6.1.7, D-088). The ROM has
 * authenticated it with pb proper's IVT, which imx-mkimage puts after the
 * plugin's CSF and loads with it (mkimage_imx8.c: "plugin can use
 * (plugin_IVT.boot_data.start + plugin_IVT.boot_data.size -
 * sizeof(imx_header_v2_t)) to get the secondary IVT pointer"). The SPL
 * brings up DRAM, has the ROM read pb proper into it and returns pb proper's
 * start, size and IVT offset; the ROM authenticates pb proper and jumps.
 * In a serial download the SPL gets pb proper from the host itself, over
 * fastboot (D-089).
 */

#include <arch/arch_helpers.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/ramlog.h>
#include <plat/imx8m/imx8m.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <plat/imx8m/spl.h>

#include "../plat.h"
#include "fb_usb.h"
#include "ivt.h"
#include "rom.h"

/*
 * The ROM's pu_irom_hwcnfg_setup(void **start, size_t *bytes, const void
 * *boot_data): entry 2 of the ROM's API table at 0x900, 64-bit pointers. Not
 * in the RM: read from the pico8ml's ROM (0xa8cc there), the function NXP's
 * i.MX6 plugins call (U-Boot, mx6_plugin.S). It asks HAB's check_target for
 * [boot_data->start, + size), copies what the ROM has read of the image and
 * reads the rest from the boot device, then writes the boot data's start and
 * size to *start and *bytes. Without a device read function, a serial
 * download, it reads nothing: the host sends the rest after the plugin.
 */
#define IMX8MQ_ROM_API_TABLE    0x900
#define IMX8MQ_ROM_HWCNFG_SETUP (IMX8MQ_ROM_API_TABLE + 2 * 8)
#define IMX8MQ_ROM_API_OKAY     0xf0

typedef uint32_t (*hwcnfg_setup_t)(void **start, size_t *bytes, const void *boot_data);

/*
 * HAB's entry and exit in its vector table (64-bit entries, U-Boot's
 * arch/arm/include/asm/mach-imx/hab.h). HAB's functions are called between
 * the two: the ROM leaves HAB before it runs the plugin, and
 * hwcnfg_setup()'s check_target outside of them fails with "function called
 * out of sequence" (HAB's event log on the pico8ml). The plugin opens HAB
 * around the call and closes it again, as the ROM had it
 */
#define HAB_RVT_ENTRY 0x08
#define HAB_RVT_EXIT  0x10
#define HAB_SUCCESS   0xf0

typedef uint32_t (*hab_call_t)(void);

/*
 * The ROM's event log (AN12853, plat-imx.md 2): its address at 0x960 on the
 * 8MQ, 64 words in the ROM's part of the OCRAM, the event's id in bits
 * 31:24. 0xd0: the ROM is in its serial downloader. hwcnfg_setup() has
 * nothing to read then and fails (0x33, read on the pico8ml): the host
 * sends pb proper after the plugin returns
 */
#define IMX8MQ_ROM_LOG_WORDS    64
#define ROM_LOG_SERIAL_DOWNLOAD 0xd0
#define ROM_LOG_END             0x00

static bool serial_download(void)
{
    uint64_t log = imx8m_rom_read_64(IMX8M_ROM_LOG_POINTER);

    if (log < IMX8M_OCRAM_BASE ||
        log + IMX8MQ_ROM_LOG_WORDS * 4 > IMX8M_OCRAM_BASE + IMX8M_OCRAM_SIZE)
        return false;

    for (unsigned int i = 0; i < IMX8MQ_ROM_LOG_WORDS; i++) {
        uint32_t id = mmio_read_32((uintptr_t)log + 4 * i) >> 24;

        if (id == ROM_LOG_SERIAL_DOWNLOAD)
            return true;
    }

    return false;
}

/* The plugin's own header, where the ROM read it: 0x40 bytes before its entry */
extern const struct imx_header spl_plugin_header;

#ifdef IMX8M_SPL_FASTBOOT
/*
 * USB recovery (D-089, F-451). The serial downloader takes one image, the
 * plugin, and can't be handed pb proper (8MQ RM 6.1.8.2.1.7): the SPL takes
 * the port from the ROM, is a fastboot device, and the host downloads the
 * image the plugin came from, pb_signed_uuu.imx, once more. Its byte 0 is
 * the plugin's IVT, so pb proper's header is 'image_offset' into it, and
 * the download goes where that puts the header at its own address.
 *
 * The step that decides what starts: the header in the download is the one
 * the ROM authenticated with the plugin, the SPL's checked copy; the
 * download reaches its CSF; and HAB authenticates pb proper as the ROM
 * would have. The SPL never goes back to the ROM from here: it starts pb
 * proper, or resets.
 */
#define RECOVERY_WDOG_TIMEOUT_s 30

static struct {
    struct imx_header hdr; /* pb proper's header, checked (imx_ivt_check()) */
    uint8_t *dest; /* Where the image's byte 0 goes */
    size_t image_offset; /* pb proper's header in the image */
} recovery;

static int recovery_accept(size_t downloaded)
{
    const struct imx_header *hdr = &recovery.hdr;
    size_t csf = hdr->ivt.csf - (uint32_t)(uintptr_t)recovery.dest;

    if (downloaded <= csf ||
        memcmp(recovery.dest + recovery.image_offset, hdr, sizeof(*hdr)) != 0) {
        LOG_ERR("fastboot: %u bytes that aren't this SPL's pb proper", (unsigned int)downloaded);
        return -PB_ERR_PARAM;
    }

    return imx8m_spl_hab_authenticate(IMX8MQ_HAB_RVT_BASE, hdr);
}

static void recovery_main(const struct imx_header *hdr, size_t image_offset)
{
    struct fb_usb_config cfg = { .accept = recovery_accept };
    uint32_t dest = hdr->ivt.self - (uint32_t)image_offset;
    int rc;

    recovery.hdr = *hdr;
    recovery.dest = (uint8_t *)(uintptr_t)dest;
    recovery.image_offset = image_offset;
    cfg.dest = recovery.dest;

    /* Above pb's buffers, as the ROM loads it (imx_ivt_check()), and whole
     * packets up to the end of pb proper's region */
    if (image_offset > hdr->ivt.self - hdr->boot_data.start) {
        LOG_ERR("fastboot: the image would start at %08x, below %08x", dest, hdr->boot_data.start);
        return;
    }

    cfg.room = ((size_t)PB_ENTRY + PB_SIZE - dest) & ~((size_t)FB_USB_PACKET - 1);

    rc = imx8m_spl_usb_init();

    if (rc == PB_OK)
        rc = fb_usb_start(&cfg);

    if (rc != PB_OK) {
        LOG_ERR("fastboot: not started (%i)", rc);
        return;
    }

    LOG_INFO("fastboot: pb proper to %08x, at most %u bytes", dest, (unsigned int)cfg.room);

    /* The watchdog from here: the session is the host's, and the step that
     * decides calls the ROM. Kicked every round, so its time is the longest
     * one round may take: HAB's authentication */
    imx8m_spl_wdog_start(RECOVERY_WDOG_TIMEOUT_s);

    do {
        imx8m_spl_wdog_kick();
        rc = fb_usb_poll();
    } while (rc == -PB_ERR_AGAIN);

    if (rc != PB_OK) {
        LOG_ERR("fastboot: ended (%i)", rc);
        return;
    }

    LOG_INFO("pb proper: %08x, from the host", hdr->ivt.entry);

    /* As before a return to the ROM: what the SPL wrote to DRAM and the RAM
     * log reach memory before pb proper, which starts with the cache off */
    dcsw_op_all(DCCSW);
    dsbsy();
    ((void (*)(void))(uintptr_t)hdr->ivt.entry)();
}
#endif

/* main() of the plugin (entry_imx8mq.S): 1, the image's values in *start,
 * *bytes and *ivt_offset; 0 when DRAM or the image's header failed, and the
 * ROM goes on to its serial downloader (8MQ RM 6.1.7) */
uint32_t spl_plugin_main(void **start, size_t *bytes, uint32_t *ivt_offset);

uint32_t spl_plugin_main(void **start, size_t *bytes, uint32_t *ivt_offset)
{
    const struct imx_header *self = &spl_plugin_header;
    const struct imx_ivt_limits limits = {
        .entry = PB_ENTRY,
        .region_end = PB_ENTRY + PB_SIZE,
        .load_floor = PB_BUFFERS_BASE + PB_BUFFERS_SIZE,
    };
    const struct imx_header *next;
    struct imx_header hdr;
    hwcnfg_setup_t hwcnfg_setup;
    hab_call_t hab_entry;
    hab_call_t hab_exit;
    uint32_t hab_rc;
    void *load_start;
    size_t load_bytes;
    uint32_t rc;

#ifdef CONFIG_RAMLOG
    ramlog_init();
#endif
    imx8m_spl_console_init();
    LOG_INFO("SPL " PB_VERSION ", SCTLR_EL3 %08lx", (unsigned long)read_sctlr_el3());

    if (imx8m_spl_dram_init() != PB_OK)
        return 0;

    /* pb proper's header, copied: what is checked is what is used */
    next = (const struct imx_header *)(uintptr_t)(self->boot_data.start + self->boot_data.size -
                                                  IMX_HEADER_SIZE);
    memcpy(&hdr, next, sizeof(hdr));

    if (imx_ivt_check(&hdr, &limits) != PB_OK)
        return 0;

    if (serial_download()) {
#ifdef IMX8M_SPL_FASTBOOT
        /* The ROM can't be handed pb proper here (F-451): a plugin that
         * returns 1 makes it go on to a pb proper the host can't have sent,
         * and it serves USB no more; 0 leaves HAB failed, and it refuses
         * every command (both read on the pico8ml). The host sends pb
         * proper to the SPL instead, and the SPL starts it or resets */
        recovery_main(&hdr, (size_t)((uintptr_t)next - (uintptr_t)self));
        LOG_ERR("SPL failed, resetting");
        imx8m_spl_reset();

        while (true)
            wfi();
#else
        /* A board without the USB port: nothing can load pb proper. 0, and
         * the ROM stays in its serial downloader */
        LOG_ERR("pb proper: no USB recovery in this build");
        dcsw_op_all(DCCSW);
        dsbsy();
        return 0;
#endif
    } else {
        hwcnfg_setup = (hwcnfg_setup_t)(uintptr_t)imx8m_rom_read_64(IMX8MQ_ROM_HWCNFG_SETUP);

        if ((uintptr_t)hwcnfg_setup >= IMX8M_ROM_SIZE) {
            LOG_ERR("ROM API: hwcnfg_setup at %p", (void *)hwcnfg_setup);
            return 0;
        }

        hab_entry = (hab_call_t)(uintptr_t)imx8m_rom_read_64(IMX8MQ_HAB_RVT_BASE + HAB_RVT_ENTRY);
        hab_exit = (hab_call_t)(uintptr_t)imx8m_rom_read_64(IMX8MQ_HAB_RVT_BASE + HAB_RVT_EXIT);

        if ((uintptr_t)hab_entry >= IMX8M_ROM_SIZE || (uintptr_t)hab_exit >= IMX8M_ROM_SIZE) {
            LOG_ERR("HAB: entry at %p, exit at %p", (void *)hab_entry, (void *)hab_exit);
            return 0;
        }

        load_start = (void *)(uintptr_t)hdr.boot_data.start;
        load_bytes = hdr.boot_data.size;
        hab_rc = hab_entry();
        rc = hwcnfg_setup(&load_start, &load_bytes, &hdr.boot_data);

        if (hab_exit() != HAB_SUCCESS || hab_rc != HAB_SUCCESS)
            LOG_ERR("HAB: entry %02x, exit failed", hab_rc);

        LOG_INFO(
            "pb proper: %08x, %u bytes, ROM %02x", hdr.boot_data.start, hdr.boot_data.size, rc);

        if (rc != IMX8MQ_ROM_API_OKAY)
            return 0;
    }

    /* The ROM authenticates pb proper at start + ivt_offset (its IVT) */
    *start = (void *)(uintptr_t)hdr.boot_data.start;
    *bytes = hdr.boot_data.size;
    *ivt_offset = hdr.ivt.self - hdr.boot_data.start;

    /* The ROM may run the plugin with its D-cache on: what the SPL wrote to
     * DRAM (TF-A's copy of the timing) and the RAM log reach memory before
     * pb proper, which starts with the cache off (F-207) */
    dcsw_op_all(DCCSW);
    dsbsy();

    return 1;
}
