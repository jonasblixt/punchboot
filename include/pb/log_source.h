/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Logs that PB_CMD_LOG_READ can read, selected by pb_id() of their name.
 * pb's RAM log ("pb", CONFIG_RAMLOG) and the list of logs ("list") are
 * built in; platforms and boards register others, e.g. the boot ROM's.
 */

#ifndef INCLUDE_PB_LOG_SOURCE_H
#define INCLUDE_PB_LOG_SOURCE_H

#include <stddef.h>
#include <stdint.h>

/**
 * A log. Positions count the bytes written to it since it started, like in
 * the RAM log: a log that doesn't change has length() == head().
 */
struct log_source {
    const char *name; /*!< Selected by pb_id(name) */
    const char *description; /*!< One line, for the list of logs */
    uint32_t (*head)(void); /*!< Bytes written to the log so far */
    size_t (*length)(void); /*!< Bytes that can be read, ending at head() */
    /** Copy out bytes from 'offset' after the oldest one that can be read,
     *  return how many */
    size_t (*read)(size_t offset, void *buf, size_t length);
};

/**
 * Add a log. Adding the same log again does nothing.
 *
 * @param[in] src The log, which must stay valid
 *
 * @return PB_OK, or -PB_ERR_MEM when there is no room for more logs
 */
int log_source_register(const struct log_source *src);

/**
 * Find a log
 *
 * @param[in] id pb_id() of its name. 0 is pb's RAM log
 *
 * @return The log, or NULL if there is no such log
 */
const struct log_source *log_source_get(uint32_t id);

#endif // INCLUDE_PB_LOG_SOURCE_H
