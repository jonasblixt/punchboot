/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The SECO's life cycle and event log on the i.MX8X, as the log "seco". The
 * SECO records a response other than success to any request (an image that
 * failed authentication, a refused fuse write) since it started; the SCFW
 * hands them out one at a time (sc_seco_get_event()), like U-Boot's
 * 'ahab_status'. Read at the first read of the log, through the SCFW.
 */

#ifndef INCLUDE_PLAT_IMX8X_SECO_LOG_H
#define INCLUDE_PLAT_IMX8X_SECO_LOG_H

#include <plat/imx8x/sci/ipc.h>
#include <stddef.h>
#include <stdint.h>

/* The most events read: the SECO keeps a few (8 on the 8QXP B0) */
#define IMX8X_SECO_LOG_MAX_EVENTS 16

/**
 * Add the log "seco"
 *
 * @param[in] ipc The SCFW's channel
 *
 * @return PB_OK, or the error from log_source_register()
 */
int imx8x_seco_log_init(sc_ipc_t ipc);

/**
 * Decode the SECO's life cycle and events into text, one line each
 *
 * @param[in] lc The life cycle (sc_seco_chip_info()), or a negative error when it wasn't read
 * @param[in] events The events (sc_seco_get_event()), oldest first
 * @param[in] n_events The number of events
 * @param[out] out The text, without a NUL
 * @param[in,out] out_len The size of out in, the text's length out
 *
 * @return PB_OK, or -PB_ERR_BUF_TOO_SMALL when the text was cut at out's size
 */
int imx8x_seco_log_decode(int32_t lc,
                          const uint32_t *events,
                          size_t n_events,
                          char *out,
                          size_t *out_len);

#endif // INCLUDE_PLAT_IMX8X_SECO_LOG_H
