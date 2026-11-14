/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_FUSE_IMX_OCOTP_H
#define INCLUDE_DRIVERS_FUSE_IMX_OCOTP_H

#include <pb/utils_def.h>
#include <stdbool.h>
#include <stdint.h>

#define OCOTP_CTRL           0x0000
#define OCOTP_CTRL_CLR       0x0008 /* ERROR is cleared here only (6UL RM 35.5.1) */
#define OCOTP_TIMING         0x0010
#define OCOTP_DATA           0x0020
#define OCOTP_READ_CTRL      0x0030
#define OCOTP_READ_FUSE_DATA 0x0040
#define OCOTP_SW_STICKY      0x0050
#define OCOTP_SCS            0x0060
#define OCOTP_SCS_SET        0x0064

/* Control register */
#define OCOTP_CTRL_BUSY      (1 << 8)
#define OCOTP_CTRL_ERROR     (1 << 9)

#define OCOTP_CTRL_WR_KEY    0x3E77

/**
 * @param base The controller
 * @param words_per_bank The fuse map's words per bank
 * @param ipg_rate_hz The controller's ipg_clk, which a write's timing is
 *        computed from. 0 when the platform doesn't give it: the fuses are
 *        read, and a write is -PB_ERR_NOT_SUPPORTED
 *
 * @return PB_OK, -PB_ERR_PARAM for words_per_bank or a rate the timing
 *         doesn't fit (above some 400 MHz)
 */
int imx_ocotp_init(uintptr_t base, unsigned int words_per_bank, unsigned int ipg_rate_hz);
int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value);
int imx_ocotp_write(uint32_t bank, uint32_t row, uint32_t value);

/* The fuse address in CTRL, and its write unlock */
#define OCOTP_CTRL_ADDR_MASK        0x7F /* The RM says 0x3F, NXP's drivers use 0x7F */
#define OCOTP_CTRL_WR_UNLOCK(key)   ((key) << 16)

/* Timing register, in ipg_clk periods */
#define OCOTP_TIMING_WAIT(x)        ((x) << 22)
#define OCOTP_TIMING_STROBE_READ(x) ((x) << 16)
#define OCOTP_TIMING_RELAX(x)       ((x) << 12)
#define OCOTP_TIMING_STROBE_PROG(x) (x)

/* By index, bank * words_per_bank + row: slc_ops' fuse_read, fuse_write */
int imx_ocotp_fuse_read(uint32_t index, uint32_t *value);
int imx_ocotp_fuse_write(uint32_t index, uint32_t value);

/* SW_STICKY: write locks, fuses and shadow registers (6UL RM 35.5.6) */
#define OCOTP_SW_STICKY_SRK_REVOKE_LOCK   BIT(1)
#define OCOTP_SW_STICKY_FIELD_RETURN_LOCK BIT(2)

/* SCS: HAB_JDE (bit 0) enables JTAG, LOCK locks the register (6UL RM 35.5.7) */
#define OCOTP_SCS_LOCK                    BIT(31)

/**
 * Lock the SW_STICKY bits in sw_sticky, and with lock_scs the SCS register.
 * Neither can be unlocked again until a reset (the RM says a power-on reset;
 * on jiffy a watchdog reset clears them too), so this is for right before the
 * jump. Read back.
 *
 * @return PB_OK, -PB_ERR_STATE before imx_ocotp_init(), -PB_ERR_IO when a
 *         lock didn't hold
 */
int imx_ocotp_lock(uint32_t sw_sticky, bool lock_scs);

#endif // INCLUDE_DRIVERS_FUSE_IMX_OCOTP_H
