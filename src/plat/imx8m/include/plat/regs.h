
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __REGS_H__
#define __REGS_H__

#define IMX8M_FUSE_SHADOW_BASE 0x30350000

#define HAB_RVT_BASE			0x00000880
#define IMX_CSU_BASE			(0x303e0000)

#define HAB_RVT_ENTRY			((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x08))
#define HAB_RVT_EXIT			((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x10))
#define HAB_RVT_AUTHENTICATE_IMAGE	((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x20))
#define HAB_RVT_REPORT_EVENT		((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x40))
#define HAB_RVT_REPORT_STATUS		((unsigned long)*(uint32_t *)(HAB_RVT_BASE + 0x48))

#endif
