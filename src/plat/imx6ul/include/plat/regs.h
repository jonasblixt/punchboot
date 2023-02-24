/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX6UL_INCLUDE_PLAT_REGS_H_
#define PLAT_IMX6UL_INCLUDE_PLAT_REGS_H_

#define HAB_RVT_BASE            0x00000100

#define HAB_RVT_ENTRY            (*(uint32_t *)(HAB_RVT_BASE + 0x04))
#define HAB_RVT_EXIT            (*(uint32_t *)(HAB_RVT_BASE + 0x08))
#define HAB_RVT_CHECK_TARGET        (*(uint32_t *)(HAB_RVT_BASE + 0x0C))
#define HAB_RVT_AUTHENTICATE_IMAGE    (*(uint32_t *)(HAB_RVT_BASE + 0x10))
#define HAB_RVT_REPORT_EVENT        (*(uint32_t *)(HAB_RVT_BASE + 0x20))
#define HAB_RVT_REPORT_STATUS        (*(uint32_t *)(HAB_RVT_BASE + 0x24))
#define HAB_RVT_FAILSAFE        (*(uint32_t *)(HAB_RVT_BASE + 0x28))

#define PAD_CTL_HYS        (1 << 16)
#define PAD_CTL_PUS_47K_UP    (1 << 14)
#define PAD_CTL_PUS_100K_UP    (2 << 14)

#define PAD_CTL_PUE        (1 << 13)
#define PAD_CTL_PKE        (1 << 12)
#define PAD_CTL_SPEED_LOW    (1 << 6)
#define PAD_CTL_SPEED_MED    (2 << 6)
#define PAD_CTL_DSE_DISABLE    (0 << 3)
#define PAD_CTL_DSE_240ohm    (1 << 3)
#define PAD_CTL_DSE_120ohm    (2 << 3)
#define PAD_CTL_DSE_80ohm    (3 << 3)
#define PAD_CTL_DSE_60ohm    (4 << 3)
#define PAD_CTL_DSE_48ohm    (5 << 3)
#define PAD_CTL_DSE_40ohm    (6 << 3)
#define PAD_CTL_DSE_34ohm    (7 << 3)
#define PAD_CTL_SRE_FAST    (1 << 0)
#define PAD_CTL_SRE_SLOW    (0 << 0)


#define UART_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |            \
        PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |               \
        PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)



#define AIPS1_ARB_BASE_ADDR             0x02000000
#define AIPS2_ARB_BASE_ADDR             0x02100000
#define ATZ1_BASE_ADDR              AIPS1_ARB_BASE_ADDR
#define ATZ2_BASE_ADDR              AIPS2_ARB_BASE_ADDR
#define AIPS1_OFF_BASE_ADDR         (ATZ1_BASE_ADDR + 0x80000)
#define ANATOP_BASE_ADDR            (AIPS1_OFF_BASE_ADDR + 0x48000)


#define UART2_BASE  (ATZ2_BASE_ADDR + 0xE8000)


#endif  // PLAT_IMX6UL_INCLUDE_PLAT_REGS_H_
