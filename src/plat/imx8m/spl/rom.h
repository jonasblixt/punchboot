/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What the i.MX8M's SPL reads of the boot ROM and has HAB do (D-088,
 * plat-imx.md 2)
 */

#ifndef PLAT_IMX8M_SPL_ROM_H
#define PLAT_IMX8M_SPL_ROM_H

#include <pb/mmio.h>
#include <stdint.h>

#include "ivt.h"

/* A word the ROM keeps at addr. The ROM is at 0: the compiler takes a
 * constant address there for NULL, so it doesn't see one */
static inline uint64_t imx8m_rom_read_64(uintptr_t addr)
{
    __asm__("" : "+r"(addr));
    return mmio_read_64(addr);
}

/**
 * HAB authenticates pb proper where it is loaded, [hdr->boot_data.start,
 * + size) with its IVT at hdr->ivt.self, as the ROM does for its own image
 * (U-Boot's imx_hab_authenticate_image()). The caller has checked the
 * header (imx_ivt_check()) and jumps to hdr->ivt.entry only on PB_OK.
 *
 * On a device that isn't closed HAB's failure is its events only, as for
 * the ROM's own image: PB_OK.
 *
 * HAB runs on its caller's stack: 2.3 KiB of the SPL's 8 KiB on the pico8ml
 * (RSA 4096, SHA-256), and 3.7 s there, the caches off as the ROM has them.
 *
 * @param[in] rvt HAB's vector table in the ROM
 * @param[in] hdr pb proper's header, the caller's checked copy
 *
 * @return PB_OK, -PB_ERR_SIGNATURE when HAB refused it on a closed device,
 *         -PB_ERR_IO when the ROM has no HAB where 'rvt' says
 */
int imx8m_spl_hab_authenticate(uintptr_t rvt, const struct imx_header *hdr);

#endif // PLAT_IMX8M_SPL_ROM_H
