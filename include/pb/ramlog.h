/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_RAMLOG_H
#define INCLUDE_PB_RAMLOG_H

#include <stddef.h>
#include <stdint.h>

#define PB_RAMLOG_MAGIC   0x474c4250 /* 'PBLG' in memory */
#define PB_RAMLOG_VERSION 1

/**
 * Header of the RAM log, followed directly by 'size' bytes of data.
 *
 * Tools outside pb read this from memory (JTAG, a boot ROM USB loader, a RAM
 * dump), so the layout is fixed: only add fields at the end, and bump the
 * version when doing so.
 */
struct pb_ramlog_hdr {
    uint32_t magic; /*!< PB_RAMLOG_MAGIC */
    uint32_t version; /*!< PB_RAMLOG_VERSION */
    uint32_t size; /*!< Size of the data area in bytes */
    uint32_t head; /*!< Bytes written since init. The next byte goes to data[head % size] */
};

/**
 * Reset the RAM log. Output written before this is dropped.
 */
void ramlog_init(void);

/**
 * Append one character to the RAM log. When the log is full, the oldest
 * character is overwritten.
 *
 * With CONFIG_RAMLOG_SYNC the character and the header are cleaned from the
 * data cache before this returns, so the log in RAM is always current.
 *
 * @param[in] c Character to append
 */
void ramlog_putc(char c);

/**
 * Number of bytes that can be read from the log.
 *
 * @return min(bytes written, log size)
 */
size_t ramlog_length(void);

/**
 * Number of bytes written to the log since ramlog_init(). The oldest byte
 * still in the log was written as byte number ramlog_head() - ramlog_length().
 *
 * @return Bytes written, 0 before ramlog_init()
 */
uint32_t ramlog_head(void);

/**
 * Copy out part of the log, oldest byte first.
 *
 * @param[in] offset Offset from the oldest byte still in the log
 * @param[out] buf Output buffer
 * @param[in] length Size of buf
 *
 * @return Number of bytes copied, 0 when offset is at or past the end
 */
size_t ramlog_read(size_t offset, void *buf, size_t length);

#endif // INCLUDE_PB_RAMLOG_H
