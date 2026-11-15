/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The step that decides whether a pb proper the SPL loaded itself may start
 * (D-088, D-089): the i.MX8M Plus's from the ROM's API, the i.MX8MQ's from
 * the SPL's fastboot. HAB's functions are the ROM's, in its vector table,
 * 64-bit entries (U-Boot, arch/arm/include/asm/mach-imx/hab.h).
 */

#include <pb/pb.h>
#include <stddef.h>

#include "rom.h"

#define HAB_RVT_HEADER        0x00
#define HAB_RVT_ENTRY         0x08
#define HAB_RVT_EXIT          0x10
#define HAB_RVT_CHECK_TARGET  0x18
#define HAB_RVT_AUTHENTICATE  0x20
#define HAB_RVT_REPORT_STATUS 0x48
#define HAB_TAG_RVT           0xdd
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
/* HAB writes its enums as 32 bit words (imx_hab_log.c, read on the boards) */
typedef uint32_t (*hab_report_status_t)(uint32_t *config, uint32_t *state);

/* A function of the table: 0 when it isn't in the ROM */
static uintptr_t rvt_function(uintptr_t rvt, unsigned int offset)
{
    uint64_t addr = imx8m_rom_read_64(rvt + offset);

    return (addr != 0 && addr < IMX8M_ROM_SIZE) ? (uintptr_t)addr : 0;
}

int imx8m_spl_hab_authenticate(uintptr_t rvt, const struct imx_header *hdr)
{
    hab_entry_t hab_entry = (hab_entry_t)rvt_function(rvt, HAB_RVT_ENTRY);
    hab_exit_t hab_exit = (hab_exit_t)rvt_function(rvt, HAB_RVT_EXIT);
    hab_check_target_t check_target = (hab_check_target_t)rvt_function(rvt, HAB_RVT_CHECK_TARGET);
    hab_authenticate_t hab_authenticate =
        (hab_authenticate_t)rvt_function(rvt, HAB_RVT_AUTHENTICATE);
    hab_report_status_t report_status =
        (hab_report_status_t)rvt_function(rvt, HAB_RVT_REPORT_STATUS);
    void *start = (void *)(uintptr_t)hdr->boot_data.start;
    size_t bytes = hdr->boot_data.size;
    ptrdiff_t ivt_offset = (ptrdiff_t)(hdr->ivt.self - hdr->boot_data.start);
    uint32_t config = 0;
    uint32_t state = 0;
    void *loaded = NULL;

    /* Nothing is called that isn't HAB's (as imx_hab_log_init()) */
    if ((imx8m_rom_read_64(rvt + HAB_RVT_HEADER) & 0xff) != HAB_TAG_RVT || hab_entry == NULL ||
        hab_exit == NULL || check_target == NULL || hab_authenticate == NULL ||
        report_status == NULL) {
        LOG_ERR("HAB: no vector table at %p", (void *)rvt);
        return -PB_ERR_IO;
    }

    LOG_DBG("HAB: %p, %u bytes, IVT at %u", start, (unsigned int)bytes, (unsigned int)ivt_offset);

    if (hab_entry() == HAB_SUCCESS && check_target(HAB_TGT_MEMORY, start, bytes) == HAB_SUCCESS)
        loaded = hab_authenticate(HAB_CID_UBOOT, ivt_offset, &start, &bytes, NULL);

    LOG_DBG("HAB: %p", loaded);

    if (hab_exit() != HAB_SUCCESS)
        loaded = NULL;

    /* HAB answers with the entry of the IVT it authenticated */
    if (loaded != (void *)(uintptr_t)hdr->ivt.entry)
        loaded = NULL;

    (void)report_status(&config, &state);

    if (loaded == NULL) {
        LOG_ERR("HAB: pb proper not authenticated (config %02x, state %02x)", config, state);

        if (config != HAB_CFG_OPEN)
            return -PB_ERR_SIGNATURE;
    }

    return PB_OK;
}
