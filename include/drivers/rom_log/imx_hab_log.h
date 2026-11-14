/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HABv4 event log of the i.MX boot ROM (i.MX6, i.MX8M, i.MX RT10xx), as
 * the log "hab": the HAB status, configuration and state, and every event
 * HAB recorded while it checked the boot image, through the ROM's HAB API
 * (report_status, report_event) in its vector table (RVT).
 */

#ifndef INCLUDE_DRIVERS_ROM_LOG_IMX_HAB_LOG_H
#define INCLUDE_DRIVERS_ROM_LOG_IMX_HAB_LOG_H

#include <stddef.h>
#include <stdint.h>

/** Where a SoC's ROM has the HAB API */
struct imx_hab_log_config {
    uintptr_t rvt; /*!< The RVT (e.g. RM "HAB API vector table addresses") */
    size_t entry_size; /*!< 4 on 32-bit SoCs, 8 on AArch64 */
};

/* The HAB API functions this log calls (HABv4 API) */
typedef uint32_t imx_hab_report_status_t(uint32_t *config, uint32_t *state);
typedef uint32_t
imx_hab_report_event_t(uint32_t status, uint32_t index, uint8_t *event, size_t *bytes);

/**
 * Make the HAB event log readable as the log "hab". The RVT's header is
 * checked first; without a valid one there is no log and nothing is called.
 *
 * @param[in] cfg Where the RVT is, must stay valid
 *
 * @return PB_OK, -PB_ERR_NOT_FOUND without an RVT, or the error from
 *         log_source_register()
 */
int imx_hab_log_init(const struct imx_hab_log_config *cfg);

/**
 * Whether HAB checked this boot's image without a failure: its status isn't
 * HAB_FAILURE and it has no failure event. Warnings pass (the i.MX8MQ's ROM
 * reports a CAAM engine failure as one). For what a failure would turn into
 * a device that doesn't boot once fused: closing it, revoking a key.
 *
 * @return PB_OK, -PB_ERR_STATE on a failure, -PB_ERR_NOT_FOUND without the
 *         HAB API (imx_hab_log_init() found no RVT)
 */
int imx_hab_no_failure(void);

/**
 * The HAB status and events as text
 *
 * @param[in] report_status The ROM's report_status
 * @param[in] report_event The ROM's report_event
 * @param[out] out Output buffer
 * @param[in,out] out_len Size of out, and the length of the text
 *
 * @return PB_OK, or -PB_ERR_BUF_TOO_SMALL if the text was truncated
 */
int imx_hab_log_decode(imx_hab_report_status_t *report_status,
                       imx_hab_report_event_t *report_event,
                       char *out,
                       size_t *out_len);

#endif // INCLUDE_DRIVERS_ROM_LOG_IMX_HAB_LOG_H
