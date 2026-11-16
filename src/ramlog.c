/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A copy of the console output in a ring buffer in RAM, for boards without
 * a UART and for reading the log after the fact.
 */

#include <pb/pb.h>
#include <pb/ramlog.h>
#include <string.h>

#define RAMLOG_SIZE PB_RAMLOG_DATA_SIZE

/* In .ramlog, so that it isn't zeroed by the startup code and is found by
 * its symbol name. The header starts a cache line, the data follows it,
 * the two CONFIG_RAMLOG_SIZE_KiB */
static struct {
    struct pb_ramlog_hdr hdr;
    char data[RAMLOG_SIZE];
} ramlog __section(".ramlog") __aligned(64) __used;

_Static_assert(sizeof(ramlog) == CONFIG_RAMLOG_SIZE_KiB * 1024,
               "The RAM log is CONFIG_RAMLOG_SIZE_KiB");

/* Set by ramlog_init(): nothing writes the log before it */
static bool ramlog_active;

/* What an earlier stage wrote of the log, which ramlog_init() kept (D-088) */
static uint32_t ramlog_kept_head;

void ramlog_init(void)
{
#if defined(PB_HAS_SPL) && !defined(PB_SPL)
    /* pb proper after the SPL (D-088): the SPL started this log at the same
     * address on this boot and pb proper goes on with it, so the SPL's lines
     * are pb's log and its console's. The SPL starts a new one every time */
    if (ramlog.hdr.magic == PB_RAMLOG_MAGIC && ramlog.hdr.version == PB_RAMLOG_VERSION &&
        ramlog.hdr.size == RAMLOG_SIZE) {
        ramlog_active = true;
        ramlog_kept_head = ramlog.hdr.head;
        return;
    }
#endif
    ramlog_kept_head = 0;
    ramlog.hdr.magic = PB_RAMLOG_MAGIC;
    ramlog.hdr.version = PB_RAMLOG_VERSION;
    ramlog.hdr.size = RAMLOG_SIZE;
    ramlog.hdr.head = 0;
    ramlog_active = true;
}

void ramlog_putc(char c)
{
    char *p;

    if (!ramlog_active || ramlog.hdr.magic != PB_RAMLOG_MAGIC)
        return;

    /* The data before the head, so that a reader never sees a head that
     * counts a character which isn't in RAM yet */
    p = &ramlog.data[ramlog.hdr.head % RAMLOG_SIZE];
    *p = c;
    ramlog.hdr.head++;
}

size_t ramlog_length(void)
{
    if (!ramlog_active || ramlog.hdr.magic != PB_RAMLOG_MAGIC)
        return 0;

    return MIN(ramlog.hdr.head, (uint32_t)RAMLOG_SIZE);
}

uint32_t ramlog_kept(void)
{
    return ramlog_active ? ramlog_kept_head : 0;
}

uint32_t ramlog_head(void)
{
    if (!ramlog_active || ramlog.hdr.magic != PB_RAMLOG_MAGIC)
        return 0;

    return ramlog.hdr.head;
}

size_t ramlog_read(size_t offset, void *buf, size_t length)
{
    size_t avail = ramlog_length();
    size_t oldest;
    size_t copied = 0;
    char *out = buf;

    if (offset >= avail)
        return 0;

    length = MIN(length, avail - offset);
    oldest = (ramlog.hdr.head - avail) % RAMLOG_SIZE;

    while (copied < length) {
        size_t pos = (oldest + offset + copied) % RAMLOG_SIZE;
        size_t chunk = MIN(length - copied, RAMLOG_SIZE - pos);

        memcpy(&out[copied], &ramlog.data[pos], chunk);
        copied += chunk;
    }

    return copied;
}
