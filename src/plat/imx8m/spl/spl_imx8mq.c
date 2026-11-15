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
#include "ivt.h"

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

/* A word the ROM keeps at addr. The ROM is at 0: the compiler takes a
 * constant address there for NULL, so it doesn't see one */
static uint64_t rom_read_64(uintptr_t addr)
{
    __asm__("" : "+r"(addr));
    return mmio_read_64(addr);
}

static bool serial_download(void)
{
    uint64_t log = rom_read_64(IMX8M_ROM_LOG_POINTER);

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
        /* A plugin that returns 1 here makes the ROM go on to pb proper,
         * which the host can't have sent yet: it serves USB no more (read
         * on the pico8ml). 0 keeps it in its serial downloader with DRAM up:
         * the host writes pb proper to DRAM and jumps to its IVT */
        LOG_INFO(
            "pb proper: %08x, %u bytes, from the host", hdr.boot_data.start, hdr.boot_data.size);
        dcsw_op_all(DCCSW);
        dsbsy();
        return 0;
    } else {
        hwcnfg_setup = (hwcnfg_setup_t)(uintptr_t)rom_read_64(IMX8MQ_ROM_HWCNFG_SETUP);

        if ((uintptr_t)hwcnfg_setup >= IMX8M_ROM_SIZE) {
            LOG_ERR("ROM API: hwcnfg_setup at %p", (void *)hwcnfg_setup);
            return 0;
        }

        hab_entry = (hab_call_t)(uintptr_t)rom_read_64(IMX8MQ_HAB_RVT_BASE + HAB_RVT_ENTRY);
        hab_exit = (hab_call_t)(uintptr_t)rom_read_64(IMX8MQ_HAB_RVT_BASE + HAB_RVT_EXIT);

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
