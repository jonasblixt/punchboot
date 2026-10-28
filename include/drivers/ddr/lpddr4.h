/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * LPDDR4 SDRAM mode register encodings (JEDEC JESD209-4), used by the
 * board timing tables.
 */

#ifndef INCLUDE_DRIVERS_DDR_LPDDR4_H
#define INCLUDE_DRIVERS_DDR_LPDDR4_H

/* ODT termination, MR11 OP[2:0] (DQ) and OP[6:4] (CA): RZQ (240 ohm) / n */
#define LPDDR4_RTT40   6
#define LPDDR4_RTT48   5
#define LPDDR4_RTT60   4
#define LPDDR4_RTT80   3
#define LPDDR4_RTT120  2
#define LPDDR4_RTT240  1
#define LPDDR4_RTT_DIS 0

/* Pull-down drive strength, MR3 OP[5:3]: RZQ / n */
#define LPDDR4_RON34   7 /* Reserved in JESD209-4, kept from NXP's reference code */
#define LPDDR4_RON40   6
#define LPDDR4_RON48   5
#define LPDDR4_RON60   4
#define LPDDR4_RON80   3

#endif // INCLUDE_DRIVERS_DDR_LPDDR4_H
