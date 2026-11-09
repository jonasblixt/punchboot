/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The kernel log that Linux's pstore/ramoops saved in RAM at its last panic,
 * oops or reboot, as the log "linux".
 */

#ifndef INCLUDE_BOOT_LINUX_RAMOOPS_H
#define INCLUDE_BOOT_LINUX_RAMOOPS_H

#include <stddef.h>
#include <stdint.h>

/** A ramoops region, as in the device tree's ramoops node */
struct linux_ramoops_config {
    uintptr_t addr; /*!< reg: the region */
    size_t size;
    size_t record_size; /*!< record-size: the dmesg records */
    size_t console_size; /*!< console-size */
    size_t ftrace_size; /*!< ftrace-size */
    size_t pmsg_size; /*!< pmsg-size */
};

/**
 * Make the newest dmesg record in a ramoops region readable as the log
 * "linux". The region is only read, when the log is.
 *
 * @param[in] cfg The region, must stay valid
 *
 * @return PB_OK, -PB_ERR_PARAM if the zones don't fit in the region or there
 *         are no records, or the error from log_source_register()
 */
int linux_ramoops_init(const struct linux_ramoops_config *cfg);

#endif // INCLUDE_BOOT_LINUX_RAMOOPS_H
